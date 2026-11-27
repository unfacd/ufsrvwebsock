/**
 * Copyright (C) 2015-2021 unfacd works
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
**
**
MODULEID("$Id: main.h,v 1.1 1999/07/26 01:46:59 ayman Exp $")
**
*/
#ifndef MAIN_H
# define MAIN_H

#define DEV_RESERVED (0x1<<0)

int   main(int, char **);
void *Thread(void *);
void  SetConfigurationDefaults(void);
void  CheckCommandLine(int, char **);
void  Help(void);

#endif
