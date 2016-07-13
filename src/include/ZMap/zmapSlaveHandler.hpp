/*  File: zmapSlaveHandler.hpp
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) 2016: Genome Research Ltd.
 *-------------------------------------------------------------------
 * ZMap is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * or see the on-line version at http://www.gnu.org/copyleft/gpl.txt
 *-------------------------------------------------------------------
 * This file is part of the ZMap genome database package
 * originally written by:
 *
 * 	Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk,
 *      Steve Miller (Sanger Institute, UK) sm23@sanger.ac.uk
 *      Gemma Barson (Sanger Institute, UK) gb10@sanger.ac.uk
 *
 * Description: Interface for initialising the interface for handling
 *              requests arriving from and replying to the slave code.
 *
 *              These calls are in fact only referenced from the
 *              slave code in zmapThreads.
 *-------------------------------------------------------------------
 */
#ifndef ZMAP_SLAVEHANDLER_H
#define ZMAP_SLAVEHANDLER_H

// Each slave handler must provide these three functions.
//
typedef ZMapThreadReturnCode (*ZMapSlaveRequestHandlerFunc)(void **slave_data, void *request_in, char **err_msg_out) ;
typedef ZMapThreadReturnCode (*ZMapSlaveTerminateHandlerFunc)(void **slave_data, char **err_msg_out) ;
typedef ZMapThreadReturnCode (*ZMapSlaveDestroyHandlerFunc)(void **slave_data) ;


// Each slave handler must provide an external callable function which the slave
// the code will call before forwarding any requests.
//
bool zMapSlaveHandlerInit(ZMapSlaveRequestHandlerFunc *handler_func_out,
                          ZMapSlaveTerminateHandlerFunc *terminate_func_out,
                          ZMapSlaveDestroyHandlerFunc *destroy_func_out) ;


bool zMapSlaveHandlerInitOld(ZMapSlaveRequestHandlerFunc *handler_func_out,
                             ZMapSlaveTerminateHandlerFunc *terminate_func_out,
                             ZMapSlaveDestroyHandlerFunc *destroy_func_out) ;




#endif /* !ZMAP_SLAVEHANDLER_H */