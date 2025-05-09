/**
 * @file
 * @brief Handles Unconfirmed COV Notifications.
 * @author Steve Karg <skarg@users.sourceforge.net>
 * @date December 2010
 * @copyright SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
/* BACnet Stack defines - first */
#include "bacnet/bacdef.h"
/* BACnet Stack API */
#include "bacnet/bacdcode.h"
#include "bacnet/apdu.h"
#include "bacnet/npdu.h"
#include "bacnet/abort.h"
#include "bacnet/cov.h"
#include "bacnet/bactext.h"
#include "bacnet/basic/services.h"
#include "bacnet/basic/sys/debug.h"
#include "bacnet/basic/tsm/tsm.h"

#ifndef MAX_COV_PROPERTIES
#define MAX_COV_PROPERTIES 2
#endif

/* COV notification callbacks list */
static BACNET_COV_NOTIFICATION Unconfirmed_COV_Notification_Head;

/**
 * @brief call the COV notification callbacks
 * @param cov_data - data decoded from the COV notification
 */
static void handler_ucov_notification_callback(BACNET_COV_DATA *cov_data)
{
    BACNET_COV_NOTIFICATION *head;

    head = &Unconfirmed_COV_Notification_Head;
    do {
        if (head->callback) {
            head->callback(cov_data);
        }
        head = head->next;
    } while (head);
}

/**
 * @brief Add a Confirmed COV notification callback
 * @param cb - COV notification callback to be added
 */
void handler_ucov_notification_add(BACNET_COV_NOTIFICATION *cb)
{
    BACNET_COV_NOTIFICATION *head;

    head = &Unconfirmed_COV_Notification_Head;
    do {
        if (head->next == cb) {
            /* already here! */
            break;
        } else if (!head->next) {
            /* first available free node */
            head->next = cb;
            break;
        }
        head = head->next;
    } while (head);
}

/**
 * @brief Print the UnconfirmedCOV data
 * @param cov_data - data decoded from the COV notification
 */
void handler_ucov_data_print(BACNET_COV_DATA *cov_data)
{
    BACNET_PROPERTY_VALUE *pProperty_value = NULL;

    debug_printf_stderr("UCOV: PID=%u ", cov_data->subscriberProcessIdentifier);
    debug_printf_stderr("instance=%u ", cov_data->initiatingDeviceIdentifier);
    debug_printf_stderr(
        "%s %u ",
        bactext_object_type_name(cov_data->monitoredObjectIdentifier.type),
        cov_data->monitoredObjectIdentifier.instance);
    debug_printf_stderr("time remaining=%u seconds ", cov_data->timeRemaining);
    debug_printf_stderr("\n");
    pProperty_value = cov_data->listOfValues;
    while (pProperty_value) {
        debug_printf_stderr("UCOV: ");
        if (pProperty_value->propertyIdentifier < 512) {
            debug_printf_stderr(
                "%s ",
                bactext_property_name(pProperty_value->propertyIdentifier));
        } else {
            debug_printf_stderr(
                "proprietary %u ", pProperty_value->propertyIdentifier);
        }
        if (pProperty_value->propertyArrayIndex != BACNET_ARRAY_ALL) {
            debug_printf_stderr("%u ", pProperty_value->propertyArrayIndex);
        }
        debug_printf_stderr("\n");
        pProperty_value = pProperty_value->next;
    }
}

/*  */
/** Handler for an Unconfirmed COV Notification.
 * @ingroup DSCOV
 * Decodes the received list of Properties to update,
 * and print them out with the subscription information.
 * @note Nothing is specified in BACnet about what to do with the
 *       information received from Unconfirmed COV Notifications.
 *
 * @param service_request [in] The contents of the service request.
 * @param service_len [in] The length of the service_request.
 * @param src [in] BACNET_ADDRESS of the source of the message (unused)
 */
void handler_ucov_notification(
    uint8_t *service_request, uint16_t service_len, BACNET_ADDRESS *src)
{
    BACNET_COV_DATA cov_data;
    BACNET_PROPERTY_VALUE property_value[MAX_COV_PROPERTIES];
    int len = 0;

    /* src not needed for this application */
    (void)src;
    /* create linked list to store data if more
       than one property value is expected */
    bacapp_property_value_list_init(&property_value[0], MAX_COV_PROPERTIES);
    cov_data.listOfValues = &property_value[0];
    debug_print("UCOV: Received Notification!\n");
    /* decode the service request only */
    len = cov_notify_decode_service_request(
        service_request, service_len, &cov_data);
    if (len > 0) {
        handler_ucov_notification_callback(&cov_data);
    } else {
        debug_print("UCOV: Unable to decode service request!\n");
    }
}
