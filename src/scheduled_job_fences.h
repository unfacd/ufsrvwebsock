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

#ifndef UFSRV_SCHEDULED_JOB_FENCES_H
#define UFSRV_SCHEDULED_JOB_FENCES_H

#include <uflib/scheduled_jobs/scheduled_jobs_type.h>

void InitialiseScheduledJobTypeForOrphanedFences(void);
ScheduledJob *GetScheduledJobForOrphanedFences(void);
int CheckOrphanedFences(ScheduledJob *, void *arg);

#endif //UFSRV_SCHEDULED_JOB_FENCES_H
