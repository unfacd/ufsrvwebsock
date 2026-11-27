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

#include "command_controllers/location_command_utils.h"
#include <ufnetcorelib/ufnet_msgcore/utils_location.h>
#include <ufnetcorelib/ufnet_msgcore/session.h>
#include <ufsrv_core/http/http_request.h>
#include <json-c/json_object.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence.h>
#include <ufnetcorelib/ufnet_core/user_prefs/user_prefs_fence.h>
#include <ufnetcorelib/ufnet_msgcore/type_providers/type_provider_fence.h>
#include "command_controllers/fence_command_controller.h"
#include "command_controllers/utils_fence_command.h"

#include <ufnetcorelib/ufnet_msgcore/misc.h>
#include <ufnetcorelib/ufnet_msgcore/json_attribute_names_account.h>
#include "nportredird.h"
#include "ufsrv_core/ufsrv_config/generated/ufsrv_config_accessor_decls_generated.h"

extern __thread ThreadContext ufsrv_thread_context;

/**
 * 	@brief: Establish user's current location based on geoip decoding.
 * 	curl http://10.1.33.1:19800/location/109.255.186.0
 * 	@note freegeoip used: http://10.1.33.1:19800/json/109.255.186.0 and minor changes to json attribute names
 */
LocationDescription *
DetermineUserLocationByServer(Session *sesn_ptr, HttpRequestContext *http_ptr, unsigned long sesn_call_flags)
{
  char *url_str = NULL;

  if (asprintf(&url_str, "http://%s:%d/location/%s", UfsrvConfigGeoipServiceAddress(), UfsrvConfigGeoipServicePort(), sesn_ptr->ufsrv_session.ssptr->haddress ) > 0) {
#ifdef __UF_TESTING
    syslog(LOG_INFO, "%s {pid:'%lu', th_ctx:'%p', o:'%p'}: Establishing LocationDescription for '%s'...", __func__, pthread_self(), THREAD_CONTEXT_PTR, sesn_ptr, url_str);
#endif

    if ((HttpRequestGetUrlJson(http_ptr, url_str, NULL, NULL)) == 0) {
      syslog(LOG_ERR, "%s {pid:'%lu', th_ctx:'%p, o:'%p'}: ERROR: COULD NOT FETCH URL: '%s'", __func__, pthread_self(), THREAD_CONTEXT_PTR, sesn_ptr, url_str);

      free(url_str);

      return NULL;
    }

#ifdef __UF_FULLDEBUG
    syslog(LOG_INFO, "%s {pid:'%lu', o:'%p'}: received response from Location Service: '%s'", __func__, pthread_self(), sesn_ptr, http_ptr->rb.memory);
#endif

    UpdateUserLocationAssignment(sesn_ptr, CLIENT_CTX_DATA(http_ptr->jobj), GEOLOC_TRIGGER_UNDEFINED, CALL_FLAG_LOCATION_BY_SERVER | CALL_FLAG_WRITEBACK_SESSION_DATA_TO_BACKEND | CALL_FLAG_BROADCAST_SESSION_EVENT);

    SESSION_ULOCATION_BYSERVER_INITIALISED(sesn_ptr) = 1;

    if (SESSION_UFSRV_GEOGROUP(sesn_ptr) == 0)	SESSION_UFSRV_GEOGROUP(sesn_ptr) = GetUfsrvGeoGroupForUser(sesn_ptr, SESSION_ULOCATION_BYSERVER(sesn_ptr).country);

    free(url_str);

    return SESSION_ULOCATION_BYSERVER_PTR(sesn_ptr);
  }

  return NULL;

}

LocationDescription *
DetermineLocationByServerByJson(Session *sesn_ptr, HttpRequestContext *http_ptr, json_object *jobj)
{
  char *url_str = NULL;
  int location_status = json_object_get_int(json__get(jobj, "status"));

  if (location_status == 2) {
    //complete processing of user location
    DetermineUserLocationByServer(sesn_ptr, GetHttpRequestContext(sesn_ptr),  0);//broadcast event

  } else if (location_status == 1) {
    double longitude = json_object_get_double(json__get(jobj, ACCOUNT_JSONATTR_LONGITUDE));
    double latitude = json_object_get_double(json__get(jobj, ACCOUNT_JSONATTR_LATITUDE));

    if (longitude && latitude) {//these values can be negative
      //TODO: perform reverse geo on these values NOT REVERSE GEO IP
      syslog(LOG_NOTICE, "%s: PARTIAL location received: longitude: '%f', latitude: '%f'", __func__, longitude, latitude);
      DetermineUserLocationByServer(sesn_ptr, GetHttpRequestContext(sesn_ptr),  0);//broadcast event
    } else {
      //some junk apparently, do reverse geo ip just as per 'status=unknown"
      syslog(LOG_NOTICE, "%s: PARTIAL INCONSISTENT location received: longitude: '%f', latitude: '%f'", __func__, longitude, latitude);
      DetermineUserLocationByServer(sesn_ptr, GetHttpRequestContext(sesn_ptr),  0);//broadcast event
    }
  } else {
    //unknown value passed
    syslog(LOG_NOTICE, "%s: LOCATION STATUS unknown: '%d'", __func__, location_status);
    DetermineUserLocationByServer(sesn_ptr, GetHttpRequestContext(sesn_ptr),  0);//broadcast event
  }

  return NULL;

}

void
UpdateUserLocationAssignmentByProto(LocationDescription *location_ptr,  LocationRecord *location_record_ptr)
{
  DestructLocationDescription(location_ptr);

  location_ptr->longitude				=	location_record_ptr->longitude;
  location_ptr->latitude				=	location_record_ptr->latitude;
  FuzzGeoLocation(location_ptr, _CONFIGDEFAULT_GEOLOC_FUZZFACTOR);

  location_ptr->country					=	IS_STR_LOADED(location_record_ptr->country)?strdup(location_record_ptr->country):NULL;
  location_ptr->admin_area			=	IS_STR_LOADED(location_record_ptr->adminarea)?strdup(location_record_ptr->adminarea):NULL;
  location_ptr->locality				=	IS_STR_LOADED(location_record_ptr->locality)?strdup(location_record_ptr->locality):NULL;
  location_ptr->last_updated		=	time(NULL);
}

////todo 9/5/25 devops: this always returns null. Results are returned in UfsrvResult
LocationDescription *
DetermineLocationByServerByWireProto(Session *sesn_ptr, const LocationRecord *location_record_ptr)
{
  char *url_str = NULL;

  //0: completely resolved, 1: partially 2: unknown. In partially we could be missing any of the address components
  if (location_record_ptr->source == LOCATION_RECORD__SOURCE__BY_UNDEFINED) {
    //complete processing of user location
    DetermineUserLocationByServer(sesn_ptr, GetHttpRequestContext(sesn_ptr),  0);//broadcast event
  } else  {
    double longitude = location_record_ptr->longitude;
    double latitude = location_record_ptr->latitude;

    if (longitude && latitude) {//these values can be negative
      //TODO: perform reverse geo on these values NOT REVERSE GEO IP
      syslog(LOG_NOTICE, "%s: PARTIAL location received: longitude: '%f', latitude: '%f'", __func__, longitude, latitude);
      DetermineUserLocationByServer(sesn_ptr, GetHttpRequestContext(sesn_ptr),  0);//broadcast event
    } else {
      //some junk apparently, do reverse geo ip just as per 'status=unknown"
      syslog(LOG_NOTICE, "%s: PARTIAL INCONSISTENT location received: longitude: '%f', latitude: '%f'", __func__, longitude, latitude);
      DetermineUserLocationByServer(sesn_ptr, GetHttpRequestContext(sesn_ptr),  0);//broadcast event
    }
  }

  return NULL;

}

/**
 * @brief given a new location state, update stored location state for the user
 * @param location_record_ptr Raw protobuf record provided by user
 * @return state describing nature of location change
 */
UFSRVResult *
UpdateLocationByWireProto(InstanceContextForSession *ctx_ptr, const LocationRecord *location_record_ptr)
{
  unsigned short origin					=	0;
  LocationDescription *loc_ptr	=	NULL;
  LocationChangeZoneMarker location_marker = {0};

//		json_bool self_zoned = json_object_get_boolean(json__get(jobj, "isSelfZoned"));
//		if (self_zoned != 0)	USER_ATTRIBUTE_SET(ctx_ptr->sesn_ptr, USERATTRIBUTE_DEFINES_USERZONE);

  //at least one reference, as either locality and admin can be null
  if (IS_STR_LOADED(location_record_ptr->country) || IS_STR_LOADED(location_record_ptr->adminarea) || IS_STR_LOADED(location_record_ptr->locality)) {
    UpdateUserLocationByUserByWireProto(AS_SESSION(ctx_ptr->sesn_ptr), location_record_ptr, &location_marker, CALL_FLAG_LOCTION_BY_USER | CALL_FLAG_WRITEBACK_SESSION_DATA_TO_BACKEND | CALL_FLAG_BROADCAST_SESSION_EVENT);
    loc_ptr = (LocationDescription *)SESSION_RESULT_USERDATA(AS_SESSION(ctx_ptr->sesn_ptr));
    origin = 2;
  } else {
    loc_ptr = DetermineLocationByServerByWireProto(AS_SESSION(ctx_ptr->sesn_ptr), location_record_ptr);
    origin = 1;
  }

  //feeds into the creation of geo base fence if one doesn't already exist
  int result;
  switch ((result = ProcessUserLocationRoaming(ctx_ptr->instance_sesn_ptr, loc_ptr, &location_marker, origin))) //<<<<<<<<<<<<<<<<<<<<<<<<<<
  {
    case LOCATION_STATE_UNCHANGED:
      _RETURN_RESULT_SESN(AS_SESSION(ctx_ptr->sesn_ptr), NULL, RESULT_TYPE_SUCCESS, RESCODE_LOCATION_UNCHANGED)

    case LOCATION_STATE_CHANGED:
    case LOCATION_STATE_INITIALISED:
      if (SESSION_RESULT_TYPE_ERROR(AS_SESSION(ctx_ptr->sesn_ptr)))	_RETURN_RESULT_SESN(AS_SESSION(ctx_ptr->sesn_ptr), NULL, RESULT_TYPE_ERR, RESCODE_PROG_NULL_POINTER)

      _RETURN_RESULT_SESN(AS_SESSION(ctx_ptr->sesn_ptr), NULL, RESULT_TYPE_SUCCESS, result == LOCATION_STATE_CHANGED? RESCODE_LOCATION_CHANGED : RESCODE_LOCATION_INIT)

    case LOCATION_STATE_UNINITIALISED:
      syslog(LOG_NOTICE, "%s (pid:'%lu',  cid:'%lu'): COULD NOT INITIALISE USER LOCATION", __func__, pthread_self(), SESSION_ID(AS_SESSION(ctx_ptr->sesn_ptr)));
      _RETURN_RESULT_SESN(AS_SESSION(ctx_ptr->sesn_ptr), NULL, RESULT_TYPE_ERR, RESCODE_LOCATION_UNINIT)

    case LOCATION_STATE_ERROR:
      syslog (LOG_NOTICE, "%s (pid:'%lu', cid:'%lu'): ERROR: COULD NOT INITIALISE USER LOCATION", __func__, pthread_self(), SESSION_ID(AS_SESSION(ctx_ptr->sesn_ptr)));
      _RETURN_RESULT_SESN(AS_SESSION(ctx_ptr->sesn_ptr), NULL, RESULT_TYPE_ERR, RESCODE_LOCATION_UNINIT)
  }

  _RETURN_RESULT_SESN(AS_SESSION(ctx_ptr->sesn_ptr), NULL, RESULT_TYPE_SUCCESS, RESCODE_LOCATION_UNINIT)
}

static void _MarkLocationChangedZones(const LocationRecord *location_record_ptr, LocationDescription *location_description_ptr, LocationChangeZoneMarker *marker_ptr);
static bool _IsAnyLocationZoneChanged(const LocationChangeZoneMarker *location_marker_ptr);
static bool _IsGeoLocRoamingZoneTriggered(InstanceHolderForSession *instance_sesn_ptr, const LocationChangeZoneMarker *location_marker_ptr);

static void
_MarkLocationChangedZones(const LocationRecord *location_record_ptr, LocationDescription *location_description_ptr, LocationChangeZoneMarker *marker_ptr) {
  const char *locality = location_record_ptr->locality;
  if (IS_STR_LOADED(locality)) {
    if (!IS_STR_LOADED(location_description_ptr->locality) || (IS_STR_LOADED(location_description_ptr->locality) && strcasecmp(locality, location_description_ptr->locality) != 0)) {//user with no location_description_ptr->locality in their session may indicate a brand new user, or user's devices relation location permission was off
      marker_ptr->locality.isLocalityChanged = true;
      marker_ptr->locality.geoloc_trigger = GEOLOC_TRIGGER_NEIGHBOURHOOD;
    }
  }

  const char *admin_area = location_record_ptr->adminarea;
  if (IS_STR_LOADED(admin_area)) {
    if (!IS_STR_LOADED(location_description_ptr->admin_area) || (IS_STR_LOADED(location_description_ptr->admin_area) && strcasecmp(admin_area, location_description_ptr->admin_area) != 0)) {
      marker_ptr->admin_area.isAdminAreaChanged = true;
      marker_ptr->admin_area.geoloc_trigger = GEOLOC_TRIGGER_REGIONHOOD;
    }
  }

  const char *country = location_record_ptr->country;
  if (IS_STR_LOADED(country)) {
    if (!IS_STR_LOADED(location_description_ptr->country) || (IS_STR_LOADED(location_description_ptr->country) && strcasecmp(country, location_description_ptr->country) != 0)) {
      marker_ptr->country.isCountryChanged = true;
      marker_ptr->country.geoloc_trigger = GEOLOC_TRIGGER_COUNTRYHOOD;
    }
  }
}

static bool
_IsAnyLocationZoneChanged(const LocationChangeZoneMarker *location_marker_ptr)
{
  return location_marker_ptr->locality.isLocalityChanged || location_marker_ptr->admin_area.isAdminAreaChanged || location_marker_ptr->country.isCountryChanged;
}

/**
 * 	@brief Main interface function for handling direct, user-initiated updates to user's location. Currently the feed-in is based on
 * 	json.
 *
 * 	@return on location change the LocationDescription is returned. If no change, NULL is returned and rescode set
 * 	to RESCODE_LOCATION_UNCHANGED
 */
UFSRVResult *
UpdateUserLocationByUserByWireProto(Session *sesn_ptr, const LocationRecord *location_record_ptr, LocationChangeZoneMarker *location_marker_ptr_out, unsigned long call_flags_sesn)
{
  _MarkLocationChangedZones(location_record_ptr, &sesn_ptr->ufnet_session.sservice.user.user_details.user_location, location_marker_ptr_out);

  if (sesn_ptr->ufnet_session.sservice.user.user_details.user_location_initialised) {
    if (_IsAnyLocationZoneChanged(location_marker_ptr_out)) {
#ifdef __UF_TESTING
      syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', cid:'%lu'}: CHANGE in user location: provided locality: '%s'. current in-use locality: '%s'. provided admin_area: '%s'. current in-use admin_area: '%s'", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), location_record_ptr->locality,  sesn_ptr->ufnet_session.sservice.user.user_details.user_location.locality, location_record_ptr->adminarea,  sesn_ptr->ufnet_session.sservice.user.user_details.user_location.admin_area);
#endif

      UpdateUserLocationAssignment(sesn_ptr, CLIENT_CTX_DATA(location_record_ptr), GEOLOC_TRIGGER_UNDEFINED, call_flags_sesn);
      UpdateHomebaseGeoLocDynamicAssignment(sesn_ptr, location_record_ptr->baseloc, call_flags_sesn);

      _RETURN_RESULT_SESN(sesn_ptr, SESSION_ULOCATION_BYUSER_PTR(sesn_ptr), RESULT_TYPE_SUCCESS, RESCODE_LOCATION_CHANGED)
    } else {
#ifdef __UF_TESTING
      syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', cid:'%lu'}: NO CHANGE in user location: passed locality: '%s'. Current in-use locality: '%s': No further processing.", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), location_record_ptr->locality, sesn_ptr->ufnet_session.sservice.user.user_details.user_location.locality);
#endif
      _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_SUCCESS, RESCODE_LOCATION_UNCHANGED)
    }
  } else {
    //this is new user session for which we never processed location prior or some complete reset.
    User *u_ptr = &(sesn_ptr->ufnet_session.sservice.user);
    UpdateUserLocationAssignment(sesn_ptr, CLIENT_CTX_DATA(location_record_ptr), GEOLOC_TRIGGER_UNDEFINED, call_flags_sesn);
    UpdateHomebaseGeoLocDynamicAssignment(sesn_ptr, location_record_ptr->baseloc, call_flags_sesn);

#ifdef __UF_TESTING
    syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu'): INITIALISE user location:  current in-use admin area: '%s'.", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), u_ptr->user_details.user_location.admin_area);
#endif

    _RETURN_RESULT_SESN(sesn_ptr, SESSION_ULOCATION_BYUSER_PTR(sesn_ptr), RESULT_TYPE_SUCCESS, RESCODE_LOCATION_CHANGED)
  }

  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_PROG_NULL_POINTER)

}

/**
 * Check if location zones have changed at all and if the user has a settable geoloc trigger value (admittedly this is not useful)
 * @param location_marker_ptr user-session specific, could be by user or by server descriptor
 * @return
 */
__unused static bool _IsGeoLocRoamingZoneTriggered(InstanceHolderForSession *instance_sesn_ptr, const LocationChangeZoneMarker *location_marker_ptr)
{
  enum GeoLocRoamingTrigger geo_loc_trigger = GetUserPrefGeolocRoamingTrigger(SessionOffInstanceHolder(instance_sesn_ptr));
  if (_IsAnyLocationZoneChanged(location_marker_ptr) && geo_loc_trigger != GEOLOC_TRIGGER_UNDEFINED) {
#ifdef __UF_TESTING
    syslog(LOG_DEBUG,"%s {pid:'%lu', cid:'%lu', zone_locality:'%d', zone_admin_area: '%d', zone_country: '%d'}: geoloc zone triggered...", __func__, pthread_self(), SessionOffInstanceHolder(instance_sesn_ptr)->ufsrv_session.session_id, location_marker_ptr->locality.isLocalityChanged, location_marker_ptr->admin_area.isAdminAreaChanged, location_marker_ptr->country.isCountryChanged);
#endif
    return true;
  }

  return false;
}


static UFSRVResult *_ResolveRoamingModeAndJoin(InstanceHolderForSession *instance_sesn_ptr, LocationDescription *location_description_ptr, const LocationChangeZoneMarker *location_marker_ptr);
static enum GeoLocRoamingTrigger _DetermineTriggeredGeolocZone(const LocationChangeZoneMarker *location_marker_ptr, enum GeoLocRoamingTrigger geoloc_trigger_loaded);
static UFSRVResult *_HandleRoamingModeWandererGeoJoin(InstanceHolderForSession *instance_sesn_ptr, LocationDescription *location_description_ptr);
static UFSRVResult *_HandleRoamingModeConquerorGeoJoin(InstanceHolderForSession *instance_sesn_ptr, LocationDescription *location_description_ptr);
static UFSRVResult *_HandleRoamingModeJournalerGeoJoin(InstanceHolderForSession *instance_sesn_ptr, LocationDescription *location_description_ptr);
static UFSRVResult *_HandleGeoJoinForWanderer(InstanceHolderForSession *instance_sesn_ptr, LocationDescription *location_description_ptr);
static inline UFSRVResult * _UpdateCurrentPastGeoFenceAssignments(Session *sesn_ptr, InstanceHolderForFence *instance_f_ptr_current, bool);


static UFSRVResult *
_ResolveRoamingModeAndJoin(InstanceHolderForSession *instance_sesn_ptr, LocationDescription *location_description_ptr, const LocationChangeZoneMarker *location_marker_ptr)
{
  UFSRVResult *res_ptr __unused;
  Session     *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);

  //TBD this determined further downstream based on LOCATION_GEOLOC_TRIGGER(location_description_ptr)	= geoloc_roaming_triggered;
//  if (!_IsGeoLocRoamingZoneTriggered(instance_sesn_ptr, location_marker_ptr)) goto return_noop;

  enum GeoLocRoamingTrigger geoloc_trigger_loaded = GetUserPrefGeolocRoamingTrigger(sesn_ptr);
  enum GeoLocRoamingTrigger geoloc_roaming_triggered = _DetermineTriggeredGeolocZone(location_marker_ptr, geoloc_trigger_loaded);//this may return GEOLOC_TRIGGER_UNDEFINED
  LOCATION_GEOLOC_TRIGGER(location_description_ptr)	= geoloc_roaming_triggered;

  if (SESSION_USERPREF_ONOFF_GET(sesn_ptr, roaming_mode)) {
    if (SESSION_USERPREF_ONOFF_GET(sesn_ptr, roaming_mode_conqueror)) {
      return (_HandleRoamingModeConquerorGeoJoin(instance_sesn_ptr, location_description_ptr));
    } else	if (SESSION_USERPREF_ONOFF_GET(sesn_ptr, roaming_mode_wanderer)) {
      return (_HandleRoamingModeWandererGeoJoin(instance_sesn_ptr, location_description_ptr));
    } else 	if	(SESSION_USERPREF_ONOFF_GET(sesn_ptr, roaming_mode_journaler)) {
      return (_HandleRoamingModeJournalerGeoJoin(instance_sesn_ptr, location_description_ptr));
    }
  }

  return_noop:
  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_NOOP, RESCODE_PROG_NULL_POINTER)
}

/**
 * @brief Based on user pref for geoloc trigger zone determine if location has changed in the same zone that the user has set pref for.
 * @param location_marker_ptr state of location under consideration
 * @param geoloc_trigger_loaded User's current pref for eoloc trigger zone
 * @return
 */
static enum GeoLocRoamingTrigger
_DetermineTriggeredGeolocZone(const LocationChangeZoneMarker *location_marker_ptr, enum GeoLocRoamingTrigger geoloc_trigger_loaded)
{
  if (location_marker_ptr->locality.isLocalityChanged && geoloc_trigger_loaded == GEOLOC_TRIGGER_NEIGHBOURHOOD) return GEOLOC_TRIGGER_NEIGHBOURHOOD;
  if (location_marker_ptr->admin_area.isAdminAreaChanged && geoloc_trigger_loaded == GEOLOC_TRIGGER_REGIONHOOD) return GEOLOC_TRIGGER_REGIONHOOD;
  if (location_marker_ptr->country.isCountryChanged && geoloc_trigger_loaded == GEOLOC_TRIGGER_COUNTRYHOOD) return GEOLOC_TRIGGER_COUNTRYHOOD;

  return GEOLOC_TRIGGER_UNDEFINED;
}

/**
 * 	@brief: In wanderer mode, we automatically remove the user from the current geo fence and lodge them into the new one
 */
static UFSRVResult *
_HandleRoamingModeWandererGeoJoin(InstanceHolderForSession *instance_sesn_ptr, LocationDescription *location_description_ptr)
{
  Fence				*f_ptr_to_join = NULL;
  InstanceHolderForFence *instance_f_ptr_to_join = NULL;
  UFSRVResult *res_ptr;

  Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);

  //TODO this condition is messy and should be rewritten. Also, we are removing user from current fence even though we have not yet determined if zone was triggered.
  /*if (IS_PRESENT(SESSION_GEOFENCE_CURRENT_INSTANCE(sesn_ptr))) {
    bool fence_lock_already_owned = false;
    size_t cname_sz = SizeofCanonicalFenceName(sesn_ptr, NULL) + 1;
    char cname[cname_sz]; memset (cname, 0, sizeof(cname));

    MakeCanonicalFenceName(sesn_ptr, NULL, USER_ATTRIBUTE_IS_SET(sesn_ptr, USERATTRIBUTE_DEFINES_USERZONE), cname);

    if ((instance_f_ptr_to_join = FindBaseFenceByCanonicalName(sesn_ptr, cname, NULL, FENCE_CALLFLAG_SEARCH_BACKEND|FENCE_CALLFLAG_HASH_FENCE_LOCALLY|FENCE_CALLFLAG_ATTACH_USER_LIST_TO_FENCE))) {
      f_ptr_to_join = FenceOffInstanceHolder(instance_f_ptr_to_join);
      if (f_ptr_to_join != SESSION_GEOFENCE_CURRENT(sesn_ptr)) {
        unsigned long eid;
        Fence 				*f_ptr_geo_current = SESSION_GEOFENCE_CURRENT(sesn_ptr);

        //we shouldn't need to lock f_ptr_geo_current before reading the pref, as the pref is property of the Session, which is locked
        if (!IsStickyGeogroupForFenceSet(sesn_ptr, f_ptr_geo_current)) {
          FenceEventsLockRWCtx(THREAD_CONTEXT_PTR, f_ptr_geo_current, _LOCK_TRY_FLAG_FALSE, SESSION_RESULT_PTR(sesn_ptr), __func__);
          if (SESSION_RESULT_TYPE_EQUAL(sesn_ptr, RESULT_TYPE_ERR)) goto return_error;
          fence_lock_already_owned = (SESSION_RESULT_CODE_EQUAL(sesn_ptr, RESCODE_PROG_LOCKED_BY_THIS_THREAD));

          if ((eid = RemoveUserFromMembersListForFence(instance_sesn_ptr, f_ptr_geo_current, FENCE_CALLFLAG_ROAMING_GEOFENCE)) != 0) {
            InstanceContextForSession instance_ctx = {instance_sesn_ptr, sesn_ptr};
            res_ptr = MarshalFenceStateSyncForLeave(&instance_ctx, &instance_ctx, &(InstanceContextForFence){SESSION_GEOFENCE_CURRENT_INSTANCE(sesn_ptr), f_ptr_geo_current}, NULL, LT_GEO_BASED);
          }

          if (!fence_lock_already_owned)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr_geo_current, SESSION_RESULT_PTR(sesn_ptr));
        }

        return (_HandleGeoJoinForWanderer(instance_sesn_ptr, location_description_ptr));
      }
      //else it seems we are cycling back into a fence that we joined once (or still members of)
    }
    //fallback below
  } else return (_HandleGeoJoinForWanderer(instance_sesn_ptr, location_description_ptr));*/

  //we are here: 1)couldn't find the Fence in the registry; OR 2)Potentially cycling back into an existing fence;
  bool fence_lock_already_owned = false;

  res_ptr = IsUserAllowedToJoinGeoFence(instance_sesn_ptr, location_description_ptr);

  if (_RESULT_TYPE_EQUAL(res_ptr, RESULT_TYPE_NOOP)) {
    return res_ptr; //maybe geoloc zone was not triggered....
  }

  if ((_RESULT_TYPE_SUCCESS(res_ptr) && _RESULT_CODE_EQUAL(res_ptr, RESCODE_USER_FENCE_JOINED)) || (_RESULT_CODE_EQUAL(res_ptr, RESCODE_USER_FENCE_ALREADYIN))) {
    instance_f_ptr_to_join = FENCESTATE_INSTANCE_HOLDER(FenceStateDescriptorOffInstanceHolder(_RESULT_USERDATA(res_ptr)));
    f_ptr_to_join = FenceOffInstanceHolder(instance_f_ptr_to_join);

    if	(IS_PRESENT(SESSION_GEOFENCE_CURRENT_INSTANCE(sesn_ptr)) && f_ptr_to_join != SESSION_GEOFENCE_CURRENT(sesn_ptr)) {
      unsigned long eid;
      Fence 				*f_ptr_geo_current = SESSION_GEOFENCE_CURRENT(sesn_ptr);

      //we shouldn't need to lock f_ptr_geo_current before reading the pref, as the pref is property of the Session, which is locked
      if (!IsStickyGeogroupForFenceSet(sesn_ptr, f_ptr_geo_current)) {
        FenceEventsLockRWCtx(THREAD_CONTEXT_PTR, f_ptr_geo_current, _LOCK_TRY_FLAG_FALSE, SESSION_RESULT_PTR(sesn_ptr), __func__);
        if (SESSION_RESULT_TYPE_EQUAL(sesn_ptr, RESULT_TYPE_ERR)) goto return_error;
        fence_lock_already_owned = (SESSION_RESULT_CODE_EQUAL(sesn_ptr, RESCODE_PROG_LOCKED_BY_THIS_THREAD));

        if ((eid = RemoveUserFromMembersListForFence(instance_sesn_ptr, f_ptr_geo_current, FENCE_CALLFLAG_ROAMING_GEOFENCE)) != 0) {
          InstanceContextForSession instance_ctx = {instance_sesn_ptr, AS_UFSRV_SESSION(sesn_ptr)};
          res_ptr = MarshalFenceStateSyncForLeave(&instance_ctx, &instance_ctx, &(InstanceContextForFence){SESSION_GEOFENCE_CURRENT_INSTANCE(sesn_ptr), f_ptr_geo_current}, NULL, LT_GEO_BASED);
        }

        if (!fence_lock_already_owned)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr_geo_current, SESSION_RESULT_PTR(sesn_ptr));
      }
    }

    return (_UpdateCurrentPastGeoFenceAssignments(sesn_ptr, instance_f_ptr_to_join, true));
  }

  return_error:
  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_PROG_NULL_POINTER)

}

/**
 * 	@brief: User just piles up geofences as they happen
 */
static UFSRVResult *
_HandleRoamingModeConquerorGeoJoin(InstanceHolderForSession *instance_sesn_ptr, LocationDescription *location_description_ptr)
{
  UFSRVResult *res_ptr;

  Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);

  res_ptr = IsUserAllowedToJoinGeoFence(instance_sesn_ptr, location_description_ptr);

  if (_RESULT_TYPE_SUCCESS(res_ptr) && (_RESULT_CODE_EQUAL(res_ptr, RESCODE_USER_FENCE_JOINED))) {
    FenceStateDescriptor *fstate_ptr  = FenceStateDescriptorOffInstanceHolder(((InstanceHolderForFenceStateDescriptor *)_RESULT_USERDATA(res_ptr)));
    return(_UpdateCurrentPastGeoFenceAssignments(sesn_ptr, FENCESTATE_INSTANCE_HOLDER(fstate_ptr), true));
  } else if (_RESULT_CODE_EQUAL(res_ptr, RESCODE_USER_FENCE_ALREADYIN)) {
    FenceStateDescriptor *fstate_ptr = FenceStateDescriptorOffInstanceHolder((InstanceHolderForFenceStateDescriptor *)_RESULT_USERDATA(res_ptr));
    return(_UpdateCurrentPastGeoFenceAssignments(sesn_ptr, FENCESTATE_INSTANCE_HOLDER(fstate_ptr), true));
  }

  return res_ptr;//outcome ofIsUserAllowed...
}

/**
 * @brief in journaler mode, user receives invitation to join triggered geofences
 * 	@locked sesn_ptr:
 * 	@locks f_ptr: NOT DIRECTLY, downstream by IsUserAllowedGeoFenceInvite
 */
static UFSRVResult *
_HandleRoamingModeJournalerGeoJoin(InstanceHolderForSession *instance_sesn_ptr, LocationDescription *location_description_ptr)
{
  Fence				*f_ptr										= NULL;
  UFSRVResult *res_ptr									=	NULL;

  Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);

  res_ptr = IsUserAllowedGeoFenceInvite(instance_sesn_ptr, NULL, GetUfsrvSystemUser(), FENCE_CALLFLAG_ROAMING_GEOFENCE);
  if (_RESULT_TYPE_SUCCESS(res_ptr)) {
    return _UpdateCurrentPastGeoFenceAssignments(sesn_ptr, (InstanceHolderForFence *)_RESULT_USERDATA(res_ptr), true);//fence unlocked
  }

  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_PROG_NULL_POINTER)
}

/**
 * 	@brief: Helper function. Assumes that SESSION_GEOFENCE_CURRENT slot has been cleaned and dereferenced
 */
__unused inline static UFSRVResult *
_HandleGeoJoinForWanderer(InstanceHolderForSession *instance_sesn_ptr, LocationDescription *location_description_ptr)
{
  Session *sesn_ptr  = SessionOffInstanceHolder(instance_sesn_ptr);

  UFSRVResult *res_ptr = IsUserAllowedToJoinGeoFence(instance_sesn_ptr, location_description_ptr);

  if ((_RESULT_TYPE_SUCCESS(res_ptr) && _RESULT_CODE_EQUAL(res_ptr, RESCODE_USER_FENCE_JOINED)) ||
      (_RESULT_CODE_EQUAL(res_ptr, RESCODE_USER_FENCE_ALREADYIN))) {
    FenceStateDescriptor *fstate_ptr = FenceStateDescriptorOffInstanceHolder((InstanceHolderForFenceStateDescriptor *)_RESULT_USERDATA(res_ptr));
    return (_UpdateCurrentPastGeoFenceAssignments(sesn_ptr, FENCESTATE_INSTANCE_HOLDER(fstate_ptr), true));
  }

  return res_ptr;//note this res is outcome of IsUserAllowedToJoinGeoFence() eg RESULT_CODE_NOOP
}


/**
 * 	@brief: Helper routine to consistently maintain the values of SESSION_GEOFENCE_CURRENT(sesn_ptr) and
 * 	 SESSION_GEOFENCE_LAST(sesn_ptr)
 */
static inline UFSRVResult *
_UpdateCurrentPastGeoFenceAssignments(Session *sesn_ptr, InstanceHolderForFence *instance_f_ptr_current, bool update_backend_flag)
{
  Fence *f_ptr_current = FenceOffInstanceHolder(instance_f_ptr_current);

  if	(IS_PRESENT(SESSION_GEOFENCE_CURRENT_INSTANCE(sesn_ptr)) && FENCE_ID(f_ptr_current) != FENCE_ID(SESSION_GEOFENCE_CURRENT(sesn_ptr))) {
    if (IS_PRESENT(SESSION_GEOFENCE_LAST_INSTANCE(sesn_ptr)))	FenceDecrementReference(SESSION_GEOFENCE_LAST_INSTANCE(sesn_ptr), 1);
    SESSION_GEOFENCE_LAST_INSTANCE(sesn_ptr) = SESSION_GEOFENCE_CURRENT_INSTANCE(sesn_ptr); //no need to increment ref as we just reuse that from old current

    SESSION_GEOFENCE_CURRENT_INSTANCE(sesn_ptr) = instance_f_ptr_current;
    FenceIncrementReference(instance_f_ptr_current, 1);

    if (update_backend_flag)	return(UpdateBackendSessionGeoJoinData(sesn_ptr, SESSION_GEOFENCE_CURRENT(sesn_ptr), SESSION_GEOFENCE_LAST(sesn_ptr)));
    else 											{_RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_SUCCESS, RESCODE_PROG_NULL_POINTER)}
  }
  //else if the same we keep as is

  if (IS_EMPTY(SESSION_GEOFENCE_CURRENT_INSTANCE(sesn_ptr))) {
    if (IS_PRESENT(SESSION_GEOFENCE_LAST_INSTANCE(sesn_ptr)) && (FenceOffInstanceHolder(instance_f_ptr_current) == SESSION_GEOFENCE_LAST(sesn_ptr))) {
      SESSION_GEOFENCE_LAST_INSTANCE(sesn_ptr) = NULL;
      SESSION_GEOFENCE_CURRENT_INSTANCE(sesn_ptr) = instance_f_ptr_current;//not incrementing ref as PAST would have had that already

      if (update_backend_flag)	return(UpdateBackendSessionGeoJoinData(sesn_ptr, SESSION_GEOFENCE_CURRENT(sesn_ptr), IS_PRESENT(SESSION_GEOFENCE_LAST_INSTANCE(sesn_ptr))?SESSION_GEOFENCE_LAST(sesn_ptr):NULL));
      else 											{_RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_SUCCESS, RESCODE_PROG_NULL_POINTER)}
    }

    SESSION_GEOFENCE_CURRENT_INSTANCE(sesn_ptr) = instance_f_ptr_current;
    FenceIncrementReference(instance_f_ptr_current, 1);

    //keep LAST as is

    if (update_backend_flag)	return(UpdateBackendSessionGeoJoinData(sesn_ptr, SESSION_GEOFENCE_CURRENT(sesn_ptr), IS_PRESENT(SESSION_GEOFENCE_LAST_INSTANCE(sesn_ptr))?SESSION_GEOFENCE_LAST(sesn_ptr):NULL));
    else 											{_RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_SUCCESS, RESCODE_PROG_NULL_POINTER)}
  }

  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_SUCCESS, RESCODE_PROG_NULL_POINTER)

}

/**
 * 	@brief Using the provided location description and its originator, determine if the user is to be joined a new geo basefance which
 * 	would happen if user's current location is different from provided one and trigger zone is triggered.
 * 	As a side effect, if user location changed, and new geo location components will have new basefences created for them.
 * 	This is the main entry point for dynamically creating basefences based on user input.
 *
 * 	TODO: this should be preference driven. At the moment is just drops user into basefences as they change relative to user's current location
 */
int
ProcessUserLocationRoaming(InstanceHolderForSession *instance_sesn_ptr, LocationDescription *loc_ptr, const LocationChangeZoneMarker *location_marker_ptr, unsigned short origin)
{
  Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);

  User *u_ptr = &(sesn_ptr->ufnet_session.sservice.user);

  if (origin == 1) {//server
    if (u_ptr->user_details.user_location_by_server_initialised) {
#ifdef __UF_TESTING
      syslog(LOG_DEBUG,"%s (pid:'%lu' cid:'%lu'): No location BY SERVER  delta  for '%s'. Returning LOCATION_STATE_UNCHANGED.", __func__, pthread_self(), sesn_ptr->ufsrv_session.session_id, u_ptr->user_details.user_name);
#endif
      return LOCATION_STATE_UNCHANGED;
    } else {
      //we only do take notice of server if user did not supply data
      //TODO: poor brittle logic
      if (!(u_ptr->user_details.user_location_initialised)) {
        //newly logged on user or user for which we have no prior history and drop user into respective base fence
#ifdef __UF_TESTING
        syslog(LOG_DEBUG, "%s (pid:'%lu' cid:'%lu'): Uninitialised user location BY SERVER '%s': Dropping user into base fence...", __func__, pthread_self(), SESSION_ID(sesn_ptr), u_ptr->user_details.user_name);
#endif

        UFSRVResult	*res_ptr = _ResolveRoamingModeAndJoin(instance_sesn_ptr, &(sesn_ptr->ufnet_session.sservice.user.user_details.user_location), location_marker_ptr);
        switch (res_ptr->result_type)
        {
          case RESULT_TYPE_SUCCESS:
            u_ptr->user_details.user_location_by_server_initialised = 1;
            return LOCATION_STATE_INITIALISED;

          case RESULT_TYPE_NOOP:
            if (res_ptr->result_code == RESCODE_USER_FENCE_ALREADYIN) {
              //this happens when user is automatically included into fence upon joining the network
              //or roaming mode is off, so no state change
              if (!u_ptr->user_details.user_location_by_server_initialised)	u_ptr->user_details.user_location_by_server_initialised = 1;
            }
            return LOCATION_STATE_UNCHANGED;

          case RESULT_TYPE_LOGICERR:
            return LOCATION_STATE_ERROR;
            //we have a fence with the user dropped in it
        }
      }
    }
  }
  else if (origin == 2) {
    //client: by this time we have in-use and last-known values set
    if (u_ptr->user_details.user_location_initialised) {
      //TODO: this is simplistic we should not consider such a literal comparison; we should allow a margin of fluctuation
      if (IS_EMPTY(loc_ptr)) {
#ifdef __UF_TESTING
        syslog(LOG_DEBUG,"%s (pid:'%lu', cid:'%lu'): No location delta  for '%s'. Returning LOCATION_STATE_UNCHANGED.", __func__,  pthread_self(), SESSION_ID(sesn_ptr), SESSION_USERNAME(sesn_ptr));
#endif
        return LOCATION_STATE_UNCHANGED;
      } else {
        Fence			*f_ptr     = NULL;
        UFSRVResult	*res_ptr = _ResolveRoamingModeAndJoin(instance_sesn_ptr, &(sesn_ptr->ufnet_session.sservice.user.user_details.user_location), location_marker_ptr);

        switch (res_ptr->result_type)
        {
          case RESULT_TYPE_SUCCESS:
            //user has been joined
            return LOCATION_STATE_CHANGED;

          case RESULT_TYPE_NOOP:
            //user already in fence
            return LOCATION_STATE_UNCHANGED;

          case RESULT_TYPE_ERR:
            return LOCATION_STATE_ERROR;

        }
      }
    } else {
      //newly logged on user or user for which we have no prior history and drop user into respective base fence
      //PROCESS_USER_LOCATION_FIELEDS;
#ifdef __UF_TESTING
      syslog(LOG_DEBUG, "%s (pid:'%lu' cid:'%lu'): Uninitialised uname='%s': Dropping user into base fence...", __func__, pthread_self(), sesn_ptr->ufsrv_session.session_id, u_ptr->user_details.user_name);
#endif

      UFSRVResult	*res_ptr = _ResolveRoamingModeAndJoin(instance_sesn_ptr, &(sesn_ptr->ufnet_session.sservice.user.user_details.user_location), location_marker_ptr);
      switch (res_ptr->result_type)
      {
        case RESULT_TYPE_SUCCESS:
          u_ptr->user_details.user_location_initialised = 1;
          u_ptr->user_details.user_location_by_server_initialised = 0;
          return LOCATION_STATE_INITIALISED;

        case RESULT_TYPE_NOOP:
          if (res_ptr->result_code == RESCODE_USER_FENCE_ALREADYIN) {
            //this happens when user is automatically included into fence upon joining the network
            if (!u_ptr->user_details.user_location_initialised) {
              u_ptr->user_details.user_location_initialised = 1;
              u_ptr->user_details.user_location_by_server_initialised = 0;
            }
          }
          return LOCATION_STATE_UNCHANGED;

        case RESULT_TYPE_ERR:
          return LOCATION_STATE_ERROR;
      }
    }
  }

  return LOCATION_STATE_UNINITIALISED;

}
