/* Lightweight partial implementation of Google's protocol buffers version 3. */

/* Copyright (C) 2020 The Regents of the University of California */

// See https://developers.google.com/protocol-buffers/docs/encoding

#include "common.h"
#include "protobuf.h"

enum pbWireType
/* "Wire type" (encoding style) of protobuf message, found in low 3 bits of first byte. */
{
    pbwtVarint=0,        // variable-size integer (int32, int64, uint32, uint64, sint32, sint64,
                         // bool, enum)
    pbwt64b=1,           // 64-bit number (fixed64, sfixed64, double)
    pbwtLengthDelim=2,   // Length-delimited (string, bytes, embedded messages,
                         // packed repeated fields)
    pbwtStartGroup=3,    // Start group (deprecated, not supported)
    pbwtEndGroup=4,      // End group (deprecated, not supported)
    pbwt32b=5,           // 32-bit number(fixed32, sfixed32, float)
    pbwtInvalid=6        // Undefined/placeholder
};

static enum pbWireType pbParseWireType(bits8 val)
/* Return the protobuf wire type encoded in the low 3 bits of val. */
{
enum pbWireType wireType = pbwtInvalid;
switch (val & 0x7)
    {
    case 0:
        wireType = pbwtVarint;
        break;
    case 1:
        wireType = pbwt64b;
        break;
    case 2:
        wireType = pbwtLengthDelim;
        break;
    case 3:
        wireType = pbwtStartGroup;
        break;
    case 4:
        wireType = pbwtEndGroup;
        break;
    case 5:
        wireType = pbwt32b;
        break;
    default:
        errAbort("Expected protobuf wire type but got %d", val);
    }
return wireType;
}

static int pbParseFieldNumberFirstByte(bits8 val)
/* Return the field number in the upper 5 bits of val. */
{
// First separate out the most significant bit
bits8 msb = val & 0x80;
val &= 0x7f;
// Shift to remove wire type bits
val = val >> 3;
// Restore the most significant bit
return (val | msb);
}

static boolean pbNextByte(FILE *stream, bits8 *retByte, long long *pBytesLeft)
/* Attempt to read a byte from stream; return FALSE if EOF.  Otherwise, store the byte in *retByte
 * and decrement *pBytesLeft, which must be greater than zero when this is called. */
{
if (*pBytesLeft <= 0)
    errAbort("pbNextByte: called when 0 bytes are left.");
int retCode = fgetc(stream);
if (retCode == EOF)
    {
    *retByte = 0;
    return FALSE;
    }
*retByte = (bits8)retCode;
(*pBytesLeft)--;
return TRUE;
}

static bits64 pbFinishVarint(FILE *stream, bits8 val, long long *pBytesLeft)
/* val is the first byte of a varint value.  While msb is set, read the next byte from stream
 * and build up the complete value of the varint.  Return the complete value. */
{
bits64 fullValue = val & 0x7f;
int byteNum = 0;
while (val & 0x80)
    {
    byteNum++;
    if (byteNum >= 10)
        errAbort("Varint reached 11th byte, shouldn't be possible.");
    if (!pbNextByte(stream, &val, pBytesLeft))
        errAbort("Unexpected EOF during varint (byte %d)", byteNum);
    fullValue |= ((val & 0x7f) << (7*byteNum));
    }
return fullValue;
}

static bits64 pbParseVarint(FILE *stream, long long *pBytesLeft)
/* Expect to find a well-formed varint in stream; parse and return. */
{
bits8 val;
if (!pbNextByte(stream, &val, pBytesLeft))
    errAbort("Expected varint, got EOF");
return pbFinishVarint(stream, val, pBytesLeft);
}

static bits64 pbParse64b(FILE *stream, long long *pBytesLeft)
/* Pull in the next 8 bytes of data from stream as a little-endian 64-bit value. */
{
bits64 fullValue = 0;
int byteNum;
for (byteNum = 0;  byteNum < 8;  byteNum++)
    {
    bits8 val;
    if (!pbNextByte(stream, &val, pBytesLeft))
        errAbort("Unexpected EOF during 64b (byte %d)", byteNum);
    fullValue |= (val << (8*byteNum));
    }
return fullValue;
}

static bits32 pbParse32b(FILE *stream, long long *pBytesLeft)
/* Pull in the next 8 bytes of data from stream as a little-endian 32-bit value. */
{
bits32 fullValue = 0;
int byteNum;
for (byteNum = 0;  byteNum < 4;  byteNum++)
    {
    bits8 val;
    if (!pbNextByte(stream, &val, pBytesLeft))
        errAbort("Unexpected EOF during 32b (byte %d)", byteNum);
    fullValue |= (val << (8*byteNum));
    }
return fullValue;
}

static char *pbParseString(FILE *stream, bits64 length, long long *pBytesLeft)
/* Return a string containing the next <length> bytes from stream. */
{
if (*pBytesLeft < length)
    errAbort("pbParseString: called with length (%llu) > bytesLeft (%lld)", length, *pBytesLeft);
char *string = needMem(length+1);
bits64 i;
for (i = 0;  i < length;  i++)
    {
    bits8 val;
    if (!pbNextByte(stream, &val, pBytesLeft))
        errAbort("Expecting string length %llu but got EOF at byte %llu", length, i);
    string[i] = val;
    }
return string;
}

static struct protobufFieldDef *protobufDefForField(struct protobufDef *proto, bits32 fieldNum)
{
struct protobufFieldDef *field;
for (field = proto->fields;  field != NULL;  field = field->next)
    if (field->fieldNum == fieldNum)
        return field;
errAbort("protobufDefForField: no field in message '%s' has number %d", proto->name, fieldNum);
return NULL;
}

static struct protobufField *pbParseField(FILE *stream, struct protobufDef *proto,
                                          long long *pBytesLeft)
/* If stream is at EOF, return NULL; otherwise parse and return the next message from stream. */
{
bits8 val;
if (!pbNextByte(stream, &val, pBytesLeft))
    return NULL;
struct protobufField *field;
AllocVar(field);
enum pbWireType wireType = pbParseWireType(val);
bits8 fieldNumB1 = pbParseFieldNumberFirstByte(val);
bits32 fieldNum = pbFinishVarint(stream, fieldNumB1, pBytesLeft);
field->def = protobufDefForField(proto, fieldNum);
enum pbDataType dataType = field->def->dataType;
verbose(2, "pbParseField: wire type %d, fieldNumber %u, field name '%s', data type %d, "
        "bytesLeft %lld\n",
        wireType, fieldNum, field->def->name, dataType, *pBytesLeft);
bits64 varint;
bits32 val32b;
int i;
switch (wireType)
    {
    case pbwtVarint:
        varint = pbParseVarint(stream, pBytesLeft);
        if (dataType == pbdtInt32)
            field->value.vInt32 = varint;
        else
            errAbort("Sorry, data type %d not yet supported", dataType);
        break;
    case pbwt64b:
        varint = pbParse64b(stream, pBytesLeft);
        if (dataType == pbdtUint64)
            field->value.vUint64 = varint;
        else
            errAbort("Sorry, data type %d not yet supported", dataType);
        break;
    case pbwtLengthDelim:
        {
        long long emByteLength = pbParseVarint(stream, pBytesLeft);
        if (*pBytesLeft < emByteLength)
            errAbort("Embedded byte length (%lld) > *pBytesLeft (%lld)",
                     emByteLength, *pBytesLeft);
        verbose(2, "Embedded with byte length %lld, data type %d, bytesLeft %lld\n",
                emByteLength, dataType, *pBytesLeft);
        if (emByteLength > 0)
            {
            long long emBytesLeft = emByteLength;
            if (dataType == pbdtString)
                {
                field->value.vString = pbParseString(stream, emByteLength, &emBytesLeft);
                field->length = emByteLength;
                }
            else if (dataType == pbdtInt32)
                {
                AllocArray(field->value.vPacked, emByteLength);
                for (i = 0;  emBytesLeft > 0;  i++)
                    {
                    field->value.vPacked[i].vInt32 = pbParseVarint(stream, &emBytesLeft);
                    verbose(2, "[%d] = %d, embedded bytes left %lld\n",
                            i, field->value.vPacked[i].vInt32, emBytesLeft);
                    }
                field->length = i;
                }
            else if (dataType == pbdtEmbedded)
                {
                verbose(2, "Expecting %lld bytes of embedded '%s' message.\n",
                        emByteLength, field->def->embedded->name);
                if (emByteLength > 0)
                    {
                    AllocArray(field->value.vEmbedded, emByteLength);
                    for (i = 0;  emBytesLeft > 0;  i++)
                        field->value.vEmbedded[i] = protobufParse(stream, field->def->embedded,
                                                                  &emBytesLeft);
                    field->length = i;
                    }
                }
            else
                errAbort("Sorry, data type %d not yet supported", dataType);
            *pBytesLeft -= emByteLength;
            }
        }
        break;
    case pbwt32b:
                val32b = pbParse32b(stream, pBytesLeft);
        if (dataType == pbdtFixed32)
            field->value.vUint32 = val32b;
        else
            errAbort("Sorry, data type %d not yet supported", dataType);
        break;
    default:
        errAbort("Unsupported wire type %d", wireType);
    }
return field;
}

struct protobuf *protobufParse(FILE *stream, struct protobufDef *proto, long long *pBytesLeft)
/* If stream is at EOF, return NULL; otherwise parse and return the next message from stream,
 * consuming up to *pBytesLeft bytes from stream.. */
{
verbose(2, "protobufParse: expecting message '%s' (or EOF), bytes left %lld\n",
        proto->name, *pBytesLeft);
struct protobufField *field = pbParseField(stream, proto, pBytesLeft);
if (field == NULL)
    return NULL;
struct protobuf *pb;
AllocVar(pb);
pb->def = proto;
pb->fields = field;
verbose(2, "protobufParse: got field %s, %lld bytes left\n", field->def->name, *pBytesLeft);
while (*pBytesLeft > 0)
    {
    field = pbParseField(stream, proto, pBytesLeft);
    if (field == NULL)
        break;
    slAddHead(&pb->fields, field);
    verbose(2, "protobufParse: got field %s, %lld bytes left\n", field->def->name, *pBytesLeft);
    }
slReverse(&pb->fields);
return pb;
}
