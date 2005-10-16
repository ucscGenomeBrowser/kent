/* frameIncr - frame increment and manipulation.  Static functions for 
 * implict inlining */
#ifndef FRAMEINCR_H
#define FRAMEINCR_H
static int frameRev(int fr)
/* reverse a frame, as if codon was reversed */
{
return (fr == 2) ? 0 : ((fr == 0) ? 2 : 1);
}

static int frameIncr(int frame, int amt)
/* increment the frame by the specified amount.  If amt is positive,
 * the frame is incremented in the direction of transcription, it 
 * it is negative it's incremented in the opposite direction. */
{
if (amt >= 0)
    return (frame + amt) % 3;
else
    return frameRev((frameRev(frame) - amt) % 3);
}

#endif
