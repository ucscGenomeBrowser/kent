/* slog - fixed point scaled logarithm stuff. */
#ifndef SLOG_H
#define SLOG_H

extern double fSlogScale;	/* Convert to fixed point by multiplying by this. */
extern double invSlogScale;	/* To convert back to floating point use this. */

int slog(double val);
/* Return scaled log. */

int carefulSlog(double val);
/* Returns scaled log that makes sure there's no int overflow. */

double invSlog(int scaledLog);
/* Inverse of slog. */

#endif /* SLOG_H */

