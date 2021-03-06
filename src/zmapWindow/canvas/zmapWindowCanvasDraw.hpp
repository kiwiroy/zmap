/*  File: zmapWindowCanvasDraw.h
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) 2006-2017: Genome Research Ltd.
 *-------------------------------------------------------------------
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------
 * This file is part of the ZMap genome database package
 * originally written by:
 * 
 *      Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk
 *        Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk
 *   Malcolm Hinsley (Sanger Institute, UK) mh17@sanger.ac.uk
 *       Gemma Guest (Sanger Institute, UK) gb10@sanger.ac.uk
 *      Steve Miller (Sanger Institute, UK) sm23@sanger.ac.uk
 *  
 * Description: Contains definitions, macros required for both canvas
 *              and Window.
 *
 *-------------------------------------------------------------------
 */
#ifndef ZMAP_CANVAS_DRAW_H
#define ZMAP_CANVAS_DRAW_H

#include <glib.h>

#include <zmapWindowAllBase.hpp>
#include <zmapWindowCanvasFeatureset.hpp>
#include <zmapWindowCanvasAlignment.hpp>


/* X Windows has some limits that are part of the protocol, this means they cannot
 * be changed any time soon...they impinge on the canvas which could have a very
 * large windows indeed. Unfortunately X just silently resets the window to size
 * 1 when you try to create larger windows....now read on...
 *
 * The largest X window that can be created has max dimensions of 65535 (i.e. an
 * unsigned short), you can easily test this for a server by creating a window and
 * then querying its size, you should find this is the max. window size you can have.
 *
 * BUT....you can't really make use of a window this size _because_ when positioning
 * anything (other windows, lines etc.), the coordinates are given via _signed_ shorts.
 * This means that the maximum position you can specify must in the range -32768
 * to 32767. In a way this makes sense because it means that you can have a window
 * that covers this entire span and so position things anywhere inside it. In a way
 * its completely crap and confusing.......
 *
 * BUT....it means that in the visible window you are limited to the range 0 - 32767.
 * by sticking at 30,000 we allow ourselves a margin for error that should work on any
 * machine.
 *
 */

#define ZMAP_WINDOW_MAX_WINDOW 	30000			    /* Largest canvas window. */

/* If this is zero why do we need it any more ??????? */
#define ZMAP_WINDOW_TEXT_BORDER 	0				/* for DNA */


/* As we zoom right in we can start to try to draw lines/boxes etc that are longer than
 * the XWindows protocol supports so we need to clamp them. */
#define CANVAS_CLAMP_COORDS(COORD)                                             \
  ((COORD) < 0 ? 0 : ((COORD) > ZMAP_WINDOW_MAX_WINDOW ? ZMAP_WINDOW_MAX_WINDOW : (COORD)))


typedef struct ColinearColoursStructType *ZMapCanvasDrawColinearColours ;



gboolean zMapWindowCanvasCalcHorizCoords(ZMapWindowFeaturesetItem featureset, ZMapWindowCanvasFeature feature,
                                         double *x1_out, double *x2_out) ;
int zMap_draw_line(GdkDrawable *drawable, ZMapWindowFeaturesetItem featureset,
                   gint cx1, gint cy1, gint cx2, gint cy2) ;

void zMap_draw_line_hack(GdkDrawable* drawable, ZMapWindowFeaturesetItem featureset,
                                gint x1, gint y1, gint x2, gint y2) ;

int zMapDrawThickLine(GdkDrawable *drawable, ZMapWindowFeaturesetItem featureset,
                         gint cx1, gint cy1, gint cx2, gint cy2) ;
int zMapDrawDashedLine(GdkDrawable *drawable, ZMapWindowFeaturesetItem featureset,
                       gint cx1, gint cy1, gint cx2, gint cy2) ;
int zMapDrawDottedLine(GdkDrawable *drawable, ZMapWindowFeaturesetItem featureset,
                       gint cx1, gint cy1, gint cx2, gint cy2) ;
int zMap_draw_rect(GdkDrawable *drawable, ZMapWindowFeaturesetItem featureset,
                   gint cx1, gint cy1, gint cx2, gint cy2, gboolean fill_flag) ;
gboolean zMapCanvasFeaturesetDrawBoxMacro(ZMapWindowFeaturesetItem featureset,
                                          double x1, double x2, double y1, double y2,
                                          GdkDrawable *drawable,
                                          gboolean fill_set, gboolean outline_set, gulong fill_flag, gulong outline) ;
void zMapCanvasFeaturesetDrawSpliceHighlights(ZMapWindowFeaturesetItem featureset, ZMapWindowCanvasFeature feature,
                                              GdkDrawable *drawable, double x1, double x2) ;

gboolean zMapCanvasDrawBoxGapped(GdkDrawable *drawable,
                                 ZMapCanvasDrawColinearColours colinear_colours,
                                 int fill_set, int outline_set,
                                 gulong ufill, gulong outline,
                                 ZMapWindowFeaturesetItem featureset, ZMapWindowCanvasFeature feature,
                                 double x1, double x2,
                                 AlignGap gapped) ;

ZMapCanvasDrawColinearColours zMapCanvasDrawAllocColinearColours(GdkColormap *colour_map) ;
ColinearityType zMapCanvasDrawGetColinearity(int end_1, int start_2, int threshold) ;
GdkColor *zMapCanvasDrawGetColinearGdkColor(ZMapCanvasDrawColinearColours colinear_colours, ColinearityType ct) ;
void zMapCanvasDrawFreeColinearColours(ZMapCanvasDrawColinearColours colinear_colours) ;


#endif /* !ZMAP_CANVAS_DRAW_H */
