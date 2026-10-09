#ifndef PQC_CSV_H
#define PQC_CSV_H
/* Tiny append CSV writer. */

typedef struct Csv Csv;

Csv *csv_open(const char *path, const char *header); /* writes header line; NULL on error */
void csv_row(Csv *c, const char *fmt, ...);          /* one formatted row (newline added)  */
void csv_close(Csv *c);

#endif /* PQC_CSV_H */
