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

#ifndef UFSRV_MESSAGE_QUEUE_VERIFICATION_CALL_FLAGS_ENUM_H
#define UFSRV_MESSAGE_QUEUE_VERIFICATION_CALL_FLAGS_ENUM_H

enum MessageQueueVerificationCallFlags {
    MSGQUEFLAG_ALLOW_SELF_PUBLISH		=	 (0x1U<<1U),
    MSGQUEFLAG_ALLOW_OTHER_TARGET		=	 (0x1U<<2U),
    MSGQUEFLAG_CHECK_RECEPIENT_UID	=	 (0x1U<<3U),
    MSGQUEFLAG_CHECK_TARGETING_ONLY	=	 (0x1U<<4U),	//only check if the broadcast intended for the right target ie no self assignments
};

#endif //UFSRV_MESSAGE_QUEUE_VERIFICATION_CALL_FLAGS_ENUM_H
