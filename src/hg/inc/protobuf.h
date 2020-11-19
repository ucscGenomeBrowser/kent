/* Lightweight partial implementation of Google's protocol buffers */

/* Copyright (C) 2020 The Regents of the University of California */

#ifndef _PROTOBUF_H_
#define _PROTOBUF_H_

enum pbDataType
/* Data type of a field of a protobuf message. */
{
    pbdtInt32,
    pbdtInt64,
    pbdtUint32,
    pbdtUint64,
    pbdtSint32,
    pbdtSint64,
    pbdtBool,
    pbdtEnum,
    pbdtFixed32,
    pbdtFixed64,
    pbdtSFixed32,
    pbdtSFixed64,
    pbdtFloat,
    pbdtDouble,
    pbdtString,
    pbdtBytes,
    pbdtEmbedded,
    pbdtPackedRepeated,
    pbdtInvalid
};

struct protobufFieldDef
/* Protobuf field definition. */
{
    struct protobufFieldDef *next;
    char *name;                   // Human-readable name of field
    bits32 fieldNum;              // Field number specified by varint in message
    enum pbDataType dataType;     // Data type (more fine-grained than wire type)
    struct protobufDef *embedded; // Definition of embedded message (if dataType is pbdtEmbedded)
    boolean isRepeated;           // TRUE if we expect a sequence of values
};

struct protobufDef
/* Protobuf message definition; fields may include other message definitions as components. */
{
    struct protobufDef *next;
    char *name;
    struct protobufFieldDef *fields;
};

union protobufFieldValue
/* The value of a protobuf field can be one of these types: */
{
    int vInt32;
    long long int vInt64;
    bits32 vUint32;
    bits64 vUint64;
    boolean vBool;
    float vFloat;
    double vDouble;
    char *vString;
    bits8 *vBytes;
    struct protobuf **vEmbedded;
    union protobufFieldValue *vPacked;
};

struct protobufField
/* One protobuf field (component of protobuf message; may include messages as components). */
{
    struct protobufField *next;
    struct protobufFieldDef *def;
    bits32 length;
    union protobufFieldValue value;
};

struct protobuf
/* One protobuf message. */
{
    struct protobuf *next;
    struct protobufDef *def;
    struct protobufField *fields;
};

struct protobuf *protobufParse(FILE *stream, struct protobufDef *proto, long long *pBytesLeft);
/* If stream is at EOF, return NULL; otherwise parse and return the next message from stream,
 * consuming up to *pBytesLeft bytes from stream.. */

#endif /* _PROTOBUF_H_ */
