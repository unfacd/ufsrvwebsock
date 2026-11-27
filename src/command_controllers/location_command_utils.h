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


#ifndef UFSRV_LOCATION_COMMAND_UTILS_H
#define UFSRV_LOCATION_COMMAND_UTILS_H


#include <json-c/json.h>
#include <ufnetcorelib/ufnet_msgcore/location/location_type.h>
#include <ufnetcorelib/ufnet_core/session/session_type.h>
#include <ufnetcorelib/ufnet_msgcore/UnfacdService.pb-c.h>
#include <ufsrv_core/delegator_sessions/http_request_context_type.h>

UFSRVResult *UpdateLocationByWireProto(InstanceContextForSession *ctx_ptr, const LocationRecord *location_record_ptr);
UFSRVResult *UpdateUserLocationByUserByWireProto(Session *sesn_ptr, const LocationRecord *location_record_ptr, LocationChangeZoneMarker *location_marker_ptr_out, unsigned long call_flags_sesn);
LocationDescription *DetermineLocationByServerByWireProto(Session *sesn_ptr, const LocationRecord *location_record_ptr);
LocationDescription *DetermineLocationByServerByJson(Session *, HttpRequestContext *, json_object *);

LocationDescription *DetermineUserLocationByServer(Session *, HttpRequestContext *, unsigned long sesn_call_flags);
UFSRVResult *UpdateUserLocationAssignment(Session *sesn_ptr, ClientContextData *, enum GeoLocRoamingTrigger geoloc_trigger, unsigned long call_flags_sesn);
void UpdateUserLocationAssignmentByProto(LocationDescription *location_ptr,  LocationRecord *location_record_ptr);
int ProcessUserLocationRoaming(InstanceHolderForSession *instance_sesn_ptr, LocationDescription *loc_ptr, const LocationChangeZoneMarker *location_marker_ptr, unsigned short origin);

//return values
enum {
    LOCATION_STATE_UNCHANGED=1, LOCATION_STATE_CHANGED, LOCATION_STATE_INITIALISED, LOCATION_STATE_UNINITIALISED, LOCATION_STATE_ERROR
};

#endif //UFSRV_LOCATION_COMMAND_UTILS_H
