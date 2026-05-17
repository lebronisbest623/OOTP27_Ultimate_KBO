#include "foreign_priority_event_audit.h"

#include "../../../core/logging/rule_audit.h"

void kbo_audit_foreign_priority_schedule(
    const char* decision,
    const char* reason,
    const char* source,
    const KboForeignPriorityEventAudit* audit)
{
    KboForeignPriorityEventAudit empty = {0};
    const KboForeignPriorityEventAudit* a = audit != 0 ? audit : &empty;
        do {
        KboLogFields audit_fields;
        kbo_log_fields_init(&audit_fields);
        kbo_log_field_u32(&audit_fields, "date", a->today);
        kbo_log_field_u32(&audit_fields, "league_id", a->league_id);
        kbo_log_field_u32(&audit_fields, "anchor_date", a->anchor_date);
        kbo_log_field_u32(&audit_fields, "open_date", a->open_date);
        kbo_log_field_u32(&audit_fields, "close_date", a->close_date);
        kbo_log_field_u32(&audit_fields, "fa_declaration_date", a->fa_declaration_date);
        kbo_log_field_u32(&audit_fields, "intl_established_fa_date", a->intl_established_fa_date);
        kbo_log_field_u32(&audit_fields, "military_selection_date", a->military_selection_date);
        kbo_log_field_i32(&audit_fields, "created_open", a->created_open);
        kbo_log_field_i32(&audit_fields, "created_close", a->created_close);
        kbo_log_field_i32(&audit_fields, "created_fa_declaration", a->created_fa_declaration);
        kbo_log_field_i32(&audit_fields, "created_intl_established_fa", a->created_intl_established_fa);
        kbo_log_field_i32(&audit_fields, "pruned_old_intl_established_fa", a->pruned_old_intl_established_fa);
        kbo_log_field_i32(&audit_fields, "created_military", a->created_military);
        kbo_log_field_i32(&audit_fields, "ready", a->ready);
        kbo_rule_audit_emit_fields(
            "custom_event.foreign_priority.schedule",
            decision,
            reason,
            source,
            &audit_fields);
    } while (0);
}
