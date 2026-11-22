#include "include/confirm.h"

#include <stddef.h>

void dhikr_confirm_begin(DhikrConfirm* confirm) {
    if(confirm == NULL) return;
    confirm->selection = DhikrConfirmChoiceNo;
}

DhikrConfirmOutcome dhikr_confirm_handle(DhikrConfirm* confirm, DhikrConfirmInput input) {
    if(confirm == NULL) return DhikrConfirmCancelled;

    switch(input) {
    case DhikrConfirmInputLeft:
        confirm->selection = DhikrConfirmChoiceNo;
        return DhikrConfirmPending;
    case DhikrConfirmInputRight:
        confirm->selection = DhikrConfirmChoiceYes;
        return DhikrConfirmPending;
    case DhikrConfirmInputOk:
        return confirm->selection == DhikrConfirmChoiceYes ? DhikrConfirmAccepted :
                                                             DhikrConfirmCancelled;
    case DhikrConfirmInputBack:
    default:
        return DhikrConfirmCancelled;
    }
}
