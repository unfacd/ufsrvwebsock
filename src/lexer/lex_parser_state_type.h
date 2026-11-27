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


#ifndef UFSRV_LEX_PARSER_STATE_TYPE_H
#define UFSRV_LEX_PARSER_STATE_TYPE_H


#ifndef FAIL_CHAR
#define FAIL_CHAR 0x08
#endif

struct LexParserState {
    short lextable_pos;
    int match_found;
};

typedef struct LexParserState LexParserState;


#endif //UFSRV_LEX_PARSER_STATE_TYPE_H
