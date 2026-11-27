/*

 Copyright (c) 2015-2026 unfacd works

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 */


#ifndef UFSRV_STAGED_MESSAGES_H
#define UFSRV_STAGED_MESSAGES_H

#include <ufsrv_core/ufsrvresult_type.h>
#include <ufnetcorelib/ufnet_core/session/session_type.h>
#include <ufnetcorelib/ufnet_core/transmission_message_type.h>
#include <ufnetcorelib/ufnet_core/incoming_message_descriptor_type.h>
#include <ufnetcorelib/ufnet_msgcore/stored_message_opttions_enum.h>
#include <ufnetcorelib/ufnet_core/ufsrv_msgcmd_type_enum.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence_event_type.h>

UFSRVResult *RemoveStagedMessageCacheRecordForIntraCommand(Session *sesn_ptr, IncomingMessageDescriptor *msg_desc_ptr,  const char *, unsigned long call_flags);
UFSRVResult *GetStagedMessageCacheRecordForIntraCommand(Session *sesn_ptr, IncomingMessageDescriptor *msg_desc_ptr, const char *payload_name, enum StoredMessageOptions msg_opts);
UFSRVResult *GetRemoveStagedMessageCacheRecordForIntraCommand(Session *sesn_ptr, IncomingMessageDescriptor *msg_desc_ptr,  const char *payload_name);

UFSRVResult *AddMessageCacheRecordForUsers(Session *sesn_ptr, IncomingMessageDescriptor *msg_desc_ptr, UfsrvMsgCommandType msg_type, const unsigned char *b64encoded_rawmsg, unsigned long call_flags);
UFSRVResult *GetMessageFromCacheRecords(Session *sesn_ptr, unsigned long call_flags);

UFSRVResult *StoreStagedMessageCacheRecordForUser(Session *sesn_ptr, TransmissionMessage *tmsg_ptr, unsigned long userid);

int DbBackendInsertMessageRecord(const ParsedMessageDescriptor *in_msg_ptr);
int DbBackendUpdateMessageStatus(unsigned long eid, unsigned  long uid_flagged_by, enum EventStatus status);

#endif //UFSRV_STAGED_MESSAGES_H
