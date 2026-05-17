#ifndef KBO_CUSTOM_EVENTS_SCHEDULES_AUDIT_FOREIGN_PRIORITY_EVENT_AUDIT_H_
#define KBO_CUSTOM_EVENTS_SCHEDULES_AUDIT_FOREIGN_PRIORITY_EVENT_AUDIT_H_

#include <stdint.h>

typedef struct KboForeignPriorityEventAudit {
    uint32_t today;
    uint32_t league_id;
    uint32_t anchor_date;
    uint32_t open_date;
    uint32_t close_date;
    uint32_t fa_declaration_date;
    uint32_t intl_established_fa_date;
    uint32_t military_selection_date;
    int created_open;
    int created_close;
    int created_fa_declaration;
    int created_intl_established_fa;
    int pruned_old_intl_established_fa;
    int created_military;
    int ready;
} KboForeignPriorityEventAudit;

void kbo_audit_foreign_priority_schedule(
    const char* decision,
    const char* reason,
    const char* source,
    const KboForeignPriorityEventAudit* audit);

#endif
