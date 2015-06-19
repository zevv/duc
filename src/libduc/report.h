
#ifndef report_h
#define report_h

duc_errno report_write(duc *duc, struct duc_index_report *rep);
struct duc_index_report *report_read(duc *duc, const char *path);

#endif

