Index: main/vcl/aqua/source/gdi/salgdi.cxx
===================================================================
--- main/vcl/aqua/source/gdi/salgdi.cxx	(revision 1232674)
+++ main/vcl/aqua/source/gdi/salgdi.cxx	(revision 1239200)
@@ -1011,10 +1011,32 @@
 		case ::basegfx::B2DLINEJOIN_BEVEL:		aCGLineJoin = kCGLineJoinBevel; break;
 		case ::basegfx::B2DLINEJOIN_MITER:		aCGLineJoin = kCGLineJoinMiter; break;
 		case ::basegfx::B2DLINEJOIN_ROUND:		aCGLineJoin = kCGLineJoinRound; break;
 	}
 
+    // setup cap attribute
+    CGLineCap aCGLineCap(kCGLineCapButt);
+
+    switch(eLineCap)
+    {
+        default: // com::sun::star::drawing::LineCap_BUTT:
+        {
+            aCGLineCap = kCGLineCapButt;
+            break;
+        }
+        case com::sun::star::drawing::LineCap_ROUND:
+        {
+            aCGLineCap = kCGLineCapRound;
+            break;
+        }
+        case com::sun::star::drawing::LineCap_SQUARE:
+        {
+            aCGLineCap = kCGLineCapSquare;
+            break;
+        }
+    }
+
 	// setup poly-polygon path
 	CGMutablePathRef xPath = CGPathCreateMutable();
 	AddPolygonToPath( xPath, rPolyLine, rPolyLine.isClosed(), !getAntiAliasB2DDraw(), true );
 
 	const CGRect aRefreshRect = CGPathGetBoundingBox( xPath );
@@ -1028,10 +1050,11 @@
         CGContextAddPath( mrContext, xPath );
         // draw path with antialiased line
         CGContextSetShouldAntialias( mrContext, true );
         CGContextSetAlpha( mrContext, 1.0 - fTransparency ); 
         CGContextSetLineJoin( mrContext, aCGLineJoin );
+        CGContextSetLineCap( mrContext, aCGLineCap );
         CGContextSetLineWidth( mrContext, rLineWidths.getX() );
         CGContextDrawPath( mrContext, kCGPathStroke );
         CGContextRestoreGState( mrContext );
         
         // mark modified rectangle as updated
Index: main/vcl/source/gdi/region.cxx
===================================================================
--- main/vcl/source/gdi/region.cxx	(revision 1232674)
+++ main/vcl/source/gdi/region.cxx	(revision 1239200)
@@ -1459,10 +1459,18 @@
         // use the PolyPolygon::Clip method for rectangles, this is
         // fairly simple (does not even use GPC) and saves us from
         // unnecessary banding
         mpImplRegion->mpPolyPoly->Clip( rRect );
 
+        // The clipping above may lead to empty ClipRegion
+        if(!mpImplRegion->mpPolyPoly->Count())
+        {
+            // react on empty ClipRegion; ImplRegion already is unique (see above)
+            delete mpImplRegion;
+            mpImplRegion = (ImplRegion*)(&aImplEmptyRegion);
+        }
+
         return sal_True;
     }
     else if( mpImplRegion->mpB2DPolyPoly )
     {
         // #127431# make ImplRegion unique, if not already.
@@ -1471,14 +1479,28 @@
             mpImplRegion->mnRefCount--;
             mpImplRegion = new ImplRegion( *mpImplRegion->mpB2DPolyPoly );
         }
 
         *mpImplRegion->mpB2DPolyPoly =
-        basegfx::tools::clipPolyPolygonOnRange( *mpImplRegion->mpB2DPolyPoly,
-                                                basegfx::B2DRange( rRect.Left(), rRect.Top(),
-                                                                   rRect.Right(), rRect.Bottom() ),
-                                                true, false );
+            basegfx::tools::clipPolyPolygonOnRange(
+                *mpImplRegion->mpB2DPolyPoly,
+                basegfx::B2DRange( 
+                    rRect.Left(), 
+                    rRect.Top(),
+                    rRect.Right() + 1, 
+                    rRect.Bottom() + 1),
+                true, 
+                false);
+
+        // The clipping above may lead to empty ClipRegion
+        if(!mpImplRegion->mpB2DPolyPoly->count())
+        {
+            // react on empty ClipRegion; ImplRegion already is unique (see above)
+            delete mpImplRegion;
+            mpImplRegion = (ImplRegion*)(&aImplEmptyRegion);
+        }
+
         return sal_True;
     }
     else
         ImplPolyPolyRegionToBandRegion();
 
@@ -2081,14 +2103,23 @@
 	// PolyPolygon data im Imp structure?
 	if ( mpImplRegion->mpPolyPoly )
 		return mpImplRegion->mpPolyPoly->GetBoundRect();
 	if( mpImplRegion->mpB2DPolyPoly )
 	{
-		const basegfx::B2DRange aRange = basegfx::tools::getRange( *mpImplRegion->mpB2DPolyPoly );
-		aRect.SetPos( Point( (int)aRange.getMinX(), (int)aRange.getMinY() ) );
-		aRect.SetSize( Size( (int)aRange.getWidth(), (int)aRange.getHeight() ) );
-		return aRect;
+		const basegfx::B2DRange aRange(basegfx::tools::getRange(*mpImplRegion->mpB2DPolyPoly));
+
+        if(aRange.isEmpty())
+        {
+            // emulate PolyPolygon::GetBoundRect() when empty polygon
+            return Rectangle();
+        }
+        else
+        {
+            return Rectangle(
+                static_cast<sal_Int32>(floor(aRange.getMinX())), static_cast<sal_Int32>(floor(aRange.getMinY())),
+                static_cast<sal_Int32>(ceil(aRange.getMaxX())), static_cast<sal_Int32>(ceil(aRange.getMaxY())));
+        }
 	}
 
 	// no band in the list? -> region is empty!
 	if ( !mpImplRegion->mpFirstBand )
 		return aRect;
Index: main/vcl/source/gdi/outdev.cxx
===================================================================
--- main/vcl/source/gdi/outdev.cxx	(revision 1232674)
+++ main/vcl/source/gdi/outdev.cxx	(revision 1239200)
@@ -996,11 +996,20 @@
                     Rectangle aDeviceBounds( mnOutOffX, mnOutOffY,
                                              mnOutOffX+GetOutputWidthPixel()-1,
                                              mnOutOffY+GetOutputHeightPixel()-1 );
                     aRegion.Intersect( aDeviceBounds );
                 }
-                ImplSelectClipRegion( aRegion );
+		        
+                if ( aRegion.IsEmpty() )
+                {
+			        mbOutputClipped = sal_True;
+                }
+		        else
+		        {
+			        mbOutputClipped = sal_False;
+			        ImplSelectClipRegion( aRegion );
+		        }
 			}
 
 			mbClipRegionSet = sal_True;
 		}
 		else
Index: main/vcl/source/gdi/svgdata.cxx
===================================================================
--- main/vcl/source/gdi/svgdata.cxx	(revision 1232674)
+++ main/vcl/source/gdi/svgdata.cxx	(revision 1239200)
@@ -28,10 +28,12 @@
 #include <com/sun/star/graphic/XSvgParser.hpp>
 #include <com/sun/star/graphic/XPrimitive2DRenderer.hpp>
 #include <com/sun/star/rendering/XIntegerReadOnlyBitmap.hpp>
 #include <vcl/canvastools.hxx>
 #include <comphelper/seqstream.hxx>
+#include <vcl/svapp.hxx>
+#include <vcl/outdev.hxx>
 
 //////////////////////////////////////////////////////////////////////////////
 
 using namespace ::com::sun::star;
 
@@ -60,17 +62,20 @@
 
                 aRealRect.X1 = rRange.getMinX();
                 aRealRect.Y1 = rRange.getMinY();
                 aRealRect.X2 = rRange.getMaxX();
                 aRealRect.Y2 = rRange.getMaxY();
-                
+
+                // get system DPI
+                const Size aDPI(Application::GetDefaultDevice()->LogicToPixel(Size(1, 1), MAP_INCH));
+
                 const uno::Reference< rendering::XBitmap > xBitmap(
                     xPrimitive2DRenderer->rasterize( 
                         maSequence,
                         aViewParameters, 
-                        72, 
-                        72, 
+                        aDPI.getWidth(), 
+                        aDPI.getHeight(), 
                         aRealRect, 
                         500000));
 
                 if(xBitmap.is())
                 {
Index: main/drawinglayer/source/primitive3d/sdrextrudelathetools3d.cxx
===================================================================
--- main/drawinglayer/source/primitive3d/sdrextrudelathetools3d.cxx	(revision 1232674)
+++ main/drawinglayer/source/primitive3d/sdrextrudelathetools3d.cxx	(revision 1239200)
@@ -114,18 +114,18 @@
 		double fTexVerStop, 
 		bool bCreateNormals, 
 		bool bCreateTextureCoordinates)
 	{
 		OSL_ENSURE(rPolA.count() == rPolB.count(), "impAddInBetweenFill: unequally sized polygons (!)");
-		const sal_uInt32 nPolygonCount(rPolA.count());
+		const sal_uInt32 nPolygonCount(::std::min(rPolA.count(), rPolB.count()));
 
 		for(sal_uInt32 a(0L); a < nPolygonCount; a++)
 		{
 			const basegfx::B3DPolygon aSubA(rPolA.getB3DPolygon(a));
 			const basegfx::B3DPolygon aSubB(rPolB.getB3DPolygon(a));
 			OSL_ENSURE(aSubA.count() == aSubB.count(), "impAddInBetweenFill: unequally sized polygons (!)");
-			const sal_uInt32 nPointCount(aSubA.count());
+			const sal_uInt32 nPointCount(::std::min(aSubA.count(), aSubB.count()));
 
 			if(nPointCount)
 			{
 				const sal_uInt32 nEdgeCount(aSubA.isClosed() ? nPointCount : nPointCount - 1L);
 				double fTexHorMultiplicatorA(0.0), fTexHorMultiplicatorB(0.0);
@@ -211,17 +211,18 @@
 		basegfx::B3DPolyPolygon& rPolA, 
 		basegfx::B3DPolyPolygon& rPolB, 
 		bool bSmoothHorizontalNormals)
 	{
 		OSL_ENSURE(rPolA.count() == rPolB.count(), "sdrExtrudePrimitive3D: unequally sized polygons (!)");
+		const sal_uInt32 nPolygonCount(::std::min(rPolA.count(), rPolB.count()));
 
-		for(sal_uInt32 a(0L); a < rPolA.count(); a++)
+		for(sal_uInt32 a(0L); a < nPolygonCount; a++)
 		{
 			basegfx::B3DPolygon aSubA(rPolA.getB3DPolygon(a));
 			basegfx::B3DPolygon aSubB(rPolB.getB3DPolygon(a));
 			OSL_ENSURE(aSubA.count() == aSubB.count(), "sdrExtrudePrimitive3D: unequally sized polygons (!)");
-			const sal_uInt32 nPointCount(aSubA.count());
+			const sal_uInt32 nPointCount(::std::min(aSubA.count(), aSubB.count()));
 
 			if(nPointCount)
 			{
 				basegfx::B3DPoint aPrevA(aSubA.getB3DPoint(nPointCount - 1L));
 				basegfx::B3DPoint aCurrA(aSubA.getB3DPoint(0L));
@@ -294,17 +295,18 @@
 		const basegfx::B3DPolyPolygon& rPolB, 
 		double fWeightA)
 	{
 		const double fWeightB(1.0 - fWeightA);
 		OSL_ENSURE(rPolA.count() == rPolB.count(), "sdrExtrudePrimitive3D: unequally sized polygons (!)");
+		const sal_uInt32 nPolygonCount(::std::min(rPolA.count(), rPolB.count()));
 
-		for(sal_uInt32 a(0L); a < rPolA.count(); a++)
+		for(sal_uInt32 a(0L); a < nPolygonCount; a++)
 		{
 			basegfx::B3DPolygon aSubA(rPolA.getB3DPolygon(a));
 			const basegfx::B3DPolygon aSubB(rPolB.getB3DPolygon(a));
 			OSL_ENSURE(aSubA.count() == aSubB.count(), "sdrExtrudePrimitive3D: unequally sized polygons (!)");
-			const sal_uInt32 nPointCount(aSubA.count());
+			const sal_uInt32 nPointCount(::std::min(aSubA.count(), aSubB.count()));
 
 			for(sal_uInt32 b(0L); b < nPointCount; b++)
 			{
 				const basegfx::B3DVector aVA(aSubA.getNormal(b) * fWeightA);
 				const basegfx::B3DVector aVB(aSubB.getNormal(b) * fWeightB);
@@ -537,16 +539,23 @@
 					{
 						basegfx::B3DPolygon aNew;
 						
 						for(sal_uInt32 d(0); d < nNumSlices; d++)
 						{
-                            OSL_ENSURE(nSlideSubPolygonCount == rSliceVector[d].getB3DPolyPolygon().count(), 
-                                "Slice PolyPolygon with different Polygon count (!)");
-                            OSL_ENSURE(nSubPolygonPointCount == rSliceVector[d].getB3DPolyPolygon().getB3DPolygon(b).count(), 
-                                "Slice Polygon with different point count (!)");
-							aNew.append(rSliceVector[d].getB3DPolyPolygon().getB3DPolygon(b).getB3DPoint(c));
-						}
+                            const bool bSamePolygonCount(nSlideSubPolygonCount == rSliceVector[d].getB3DPolyPolygon().count());
+                            const bool bSamePointCount(nSubPolygonPointCount == rSliceVector[d].getB3DPolyPolygon().getB3DPolygon(b).count());
+
+                            if(bSamePolygonCount && bSamePointCount)
+                            {
+                                aNew.append(rSliceVector[d].getB3DPolyPolygon().getB3DPolygon(b).getB3DPoint(c));
+                            }
+                            else
+                            {
+                                OSL_ENSURE(bSamePolygonCount, "Slice PolyPolygon with different Polygon count (!)");
+                                OSL_ENSURE(bSamePointCount, "Slice Polygon with different point count (!)");
+                            }
+                        }
 
 						aNew.setClosed(bCloseHorLines);
 						aRetval.append(aNew);
 					}
 				}
Index: main/drawinglayer/source/primitive3d/sdrlatheprimitive3d.cxx
===================================================================
--- main/drawinglayer/source/primitive3d/sdrlatheprimitive3d.cxx	(revision 1232674)
+++ main/drawinglayer/source/primitive3d/sdrlatheprimitive3d.cxx	(revision 1239200)
@@ -211,10 +211,11 @@
 
 		void SdrLathePrimitive3D::impCreateSlices()
 		{
 			// prepare the polygon. No double points, correct orientations and a correct
 			// outmost polygon are needed
+            // Also important: subdivide here to ensure equal point count for all slices (!)
 			maCorrectedPolyPolygon = basegfx::tools::adaptiveSubdivideByAngle(getPolyPolygon());
 			maCorrectedPolyPolygon.removeDoublePoints();
 			maCorrectedPolyPolygon = basegfx::tools::correctOrientations(maCorrectedPolyPolygon);
 			maCorrectedPolyPolygon = basegfx::tools::correctOutmostPolygon(maCorrectedPolyPolygon);
 
Index: main/drawinglayer/source/primitive3d/hiddengeometryprimitive3d.cxx
===================================================================
--- main/drawinglayer/source/primitive3d/hiddengeometryprimitive3d.cxx	(revision 1232674)
+++ main/drawinglayer/source/primitive3d/hiddengeometryprimitive3d.cxx	(revision 1239200)
@@ -17,45 +17,10 @@
  * specific language governing permissions and limitations
  * under the License.
  * 
  *************************************************************/
 
-/*************************************************************************
- *
- *  OpenOffice.org - a multi-platform office productivity suite
- *
- *  $RCSfile: hittestprimitive3d.cxx,v $
- *
- *  $Revision: 1.1.2.1 $
- *
- *  last change: $Author: aw $ $Date: 2008/09/25 17:12:14 $
- *
- *  The Contents of this file are made available subject to
- *  the terms of GNU Lesser General Public License Version 2.1.
- *
- *
- *    GNU Lesser General Public License Version 2.1
- *    =============================================
- *    Copyright 2005 by Sun Microsystems, Inc.
- *    901 San Antonio Road, Palo Alto, CA 94303, USA
- *
- *    This library is free software; you can redistribute it and/or
- *    modify it under the terms of the GNU Lesser General Public
- *    License version 2.1, as published by the Free Software Foundation.
- *
- *    This library is distributed in the hope that it will be useful,
- *    but WITHOUT ANY WARRANTY; without even the implied warranty of
- *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
- *    Lesser General Public License for more details.
- *
- *    You should have received a copy of the GNU Lesser General Public
- *    License along with this library; if not, write to the Free Software
- *    Foundation, Inc., 59 Temple Place, Suite 330, Boston,
- *    MA  02111-1307  USA
- *
- ************************************************************************/
-
 // MARKER(update_precomp.py): autogen include statement, do not remove
 #include "precompiled_drawinglayer.hxx"
 
 #include <drawinglayer/primitive3d/hiddengeometryprimitive3d.hxx>
 #include <drawinglayer/primitive3d/drawinglayer_primitivetypes3d.hxx>
Index: main/drawinglayer/source/primitive3d/sdrextrudeprimitive3d.cxx
===================================================================
--- main/drawinglayer/source/primitive3d/sdrextrudeprimitive3d.cxx	(revision 1232674)
+++ main/drawinglayer/source/primitive3d/sdrextrudeprimitive3d.cxx	(revision 1239200)
@@ -367,11 +367,12 @@
 
 		void SdrExtrudePrimitive3D::impCreateSlices()
 		{
 			// prepare the polygon. No double points, correct orientations and a correct
 			// outmost polygon are needed
-			maCorrectedPolyPolygon = getPolyPolygon();
+            // Also important: subdivide here to ensure equal point count for all slices (!)
+			maCorrectedPolyPolygon = basegfx::tools::adaptiveSubdivideByAngle(getPolyPolygon());
 			maCorrectedPolyPolygon.removeDoublePoints();
 			maCorrectedPolyPolygon = basegfx::tools::correctOrientations(maCorrectedPolyPolygon);
 			maCorrectedPolyPolygon = basegfx::tools::correctOutmostPolygon(maCorrectedPolyPolygon);
 
 			// prepare slices as geometry
Index: main/drawinglayer/source/attribute/sdrsceneattribute3d.cxx
===================================================================
--- main/drawinglayer/source/attribute/sdrsceneattribute3d.cxx	(revision 1232674)
+++ main/drawinglayer/source/attribute/sdrsceneattribute3d.cxx	(revision 1239200)
@@ -1,39 +1,25 @@
-/*************************************************************************
- *
- *  OpenOffice.org - a multi-platform office productivity suite
- *
- *  $RCSfile: sdrattribute3d.cxx,v $
- *
- *  $Revision: 1.5 $
- *
- *  last change: $Author: aw $ $Date: 2008-05-27 14:11:19 $
- *
- *  The Contents of this file are made available subject to
- *  the terms of GNU Lesser General Public License Version 2.1.
- *
- *
- *    GNU Lesser General Public License Version 2.1
- *    =============================================
- *    Copyright 2005 by Sun Microsystems, Inc.
- *    901 San Antonio Road, Palo Alto, CA 94303, USA
- *
- *    This library is free software; you can redistribute it and/or
- *    modify it under the terms of the GNU Lesser General Public
- *    License version 2.1, as published by the Free Software Foundation.
- *
- *    This library is distributed in the hope that it will be useful,
- *    but WITHOUT ANY WARRANTY; without even the implied warranty of
- *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
- *    Lesser General Public License for more details.
- *
- *    You should have received a copy of the GNU Lesser General Public
- *    License along with this library; if not, write to the Free Software
- *    Foundation, Inc., 59 Temple Place, Suite 330, Boston,
- *    MA  02111-1307  USA
- *
- ************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
 
 // MARKER(update_precomp.py): autogen include statement, do not remove
 #include "precompiled_drawinglayer.hxx"
 
 #include <drawinglayer/attribute/sdrsceneattribute3d.hxx>
Index: main/drawinglayer/source/attribute/sdrshadowattribute.cxx
===================================================================
--- main/drawinglayer/source/attribute/sdrshadowattribute.cxx	(revision 1232674)
+++ main/drawinglayer/source/attribute/sdrshadowattribute.cxx	(revision 1239200)
@@ -1,39 +1,25 @@
-/*************************************************************************
- *
- *  OpenOffice.org - a multi-platform office productivity suite
- *
- *  $RCSfile: sdrattribute.cxx,v $
- *
- *  $Revision: 1.5 $
- *
- *  last change: $Author: aw $ $Date: 2008-05-27 14:11:19 $
- *
- *  The Contents of this file are made available subject to
- *  the terms of GNU Lesser General Public License Version 2.1.
- *
- *
- *    GNU Lesser General Public License Version 2.1
- *    =============================================
- *    Copyright 2005 by Sun Microsystems, Inc.
- *    901 San Antonio Road, Palo Alto, CA 94303, USA
- *
- *    This library is free software; you can redistribute it and/or
- *    modify it under the terms of the GNU Lesser General Public
- *    License version 2.1, as published by the Free Software Foundation.
- *
- *    This library is distributed in the hope that it will be useful,
- *    but WITHOUT ANY WARRANTY; without even the implied warranty of
- *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
- *    Lesser General Public License for more details.
- *
- *    You should have received a copy of the GNU Lesser General Public
- *    License along with this library; if not, write to the Free Software
- *    Foundation, Inc., 59 Temple Place, Suite 330, Boston,
- *    MA  02111-1307  USA
- *
- ************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
 
 // MARKER(update_precomp.py): autogen include statement, do not remove
 #include "precompiled_drawinglayer.hxx"
 
 #include <drawinglayer/attribute/sdrshadowattribute.hxx>
Index: main/drawinglayer/source/attribute/fillhatchattribute.cxx
===================================================================
--- main/drawinglayer/source/attribute/fillhatchattribute.cxx	(revision 1232674)
+++ main/drawinglayer/source/attribute/fillhatchattribute.cxx	(revision 1239200)
@@ -1,39 +1,25 @@
-/*************************************************************************
- *
- *  OpenOffice.org - a multi-platform office productivity suite
- *
- *  $RCSfile: fillattribute.cxx,v $
- *
- *  $Revision: 1.4 $
- *
- *  last change: $Author: aw $ $Date: 2008-05-27 14:11:19 $
- *
- *  The Contents of this file are made available subject to
- *  the terms of GNU Lesser General Public License Version 2.1.
- *
- *
- *    GNU Lesser General Public License Version 2.1
- *    =============================================
- *    Copyright 2005 by Sun Microsystems, Inc.
- *    901 San Antonio Road, Palo Alto, CA 94303, USA
- *
- *    This library is free software; you can redistribute it and/or
- *    modify it under the terms of the GNU Lesser General Public
- *    License version 2.1, as published by the Free Software Foundation.
- *
- *    This library is distributed in the hope that it will be useful,
- *    but WITHOUT ANY WARRANTY; without even the implied warranty of
- *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
- *    Lesser General Public License for more details.
- *
- *    You should have received a copy of the GNU Lesser General Public
- *    License along with this library; if not, write to the Free Software
- *    Foundation, Inc., 59 Temple Place, Suite 330, Boston,
- *    MA  02111-1307  USA
- *
- ************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
 
 // MARKER(update_precomp.py): autogen include statement, do not remove
 #include "precompiled_drawinglayer.hxx"
 
 #include <drawinglayer/attribute/fillhatchattribute.hxx>
Index: main/drawinglayer/source/attribute/sdrfillattribute.cxx
===================================================================
--- main/drawinglayer/source/attribute/sdrfillattribute.cxx	(revision 1232674)
+++ main/drawinglayer/source/attribute/sdrfillattribute.cxx	(revision 1239200)
@@ -1,39 +1,25 @@
-/*************************************************************************
- *
- *  OpenOffice.org - a multi-platform office productivity suite
- *
- *  $RCSfile: sdrattribute.cxx,v $
- *
- *  $Revision: 1.5 $
- *
- *  last change: $Author: aw $ $Date: 2008-05-27 14:11:19 $
- *
- *  The Contents of this file are made available subject to
- *  the terms of GNU Lesser General Public License Version 2.1.
- *
- *
- *    GNU Lesser General Public License Version 2.1
- *    =============================================
- *    Copyright 2005 by Sun Microsystems, Inc.
- *    901 San Antonio Road, Palo Alto, CA 94303, USA
- *
- *    This library is free software; you can redistribute it and/or
- *    modify it under the terms of the GNU Lesser General Public
- *    License version 2.1, as published by the Free Software Foundation.
- *
- *    This library is distributed in the hope that it will be useful,
- *    but WITHOUT ANY WARRANTY; without even the implied warranty of
- *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
- *    Lesser General Public License for more details.
- *
- *    You should have received a copy of the GNU Lesser General Public
- *    License along with this library; if not, write to the Free Software
- *    Foundation, Inc., 59 Temple Place, Suite 330, Boston,
- *    MA  02111-1307  USA
- *
- ************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
 
 // MARKER(update_precomp.py): autogen include statement, do not remove
 #include "precompiled_drawinglayer.hxx"
 
 #include <drawinglayer/attribute/sdrfillattribute.hxx>
Index: main/drawinglayer/source/attribute/sdrlineattribute.cxx
===================================================================
--- main/drawinglayer/source/attribute/sdrlineattribute.cxx	(revision 1232674)
+++ main/drawinglayer/source/attribute/sdrlineattribute.cxx	(revision 1239200)
@@ -1,39 +1,25 @@
-/*************************************************************************
- *
- *  OpenOffice.org - a multi-platform office productivity suite
- *
- *  $RCSfile: sdrattribute.cxx,v $
- *
- *  $Revision: 1.5 $
- *
- *  last change: $Author: aw $ $Date: 2008-05-27 14:11:19 $
- *
- *  The Contents of this file are made available subject to
- *  the terms of GNU Lesser General Public License Version 2.1.
- *
- *
- *    GNU Lesser General Public License Version 2.1
- *    =============================================
- *    Copyright 2005 by Sun Microsystems, Inc.
- *    901 San Antonio Road, Palo Alto, CA 94303, USA
- *
- *    This library is free software; you can redistribute it and/or
- *    modify it under the terms of the GNU Lesser General Public
- *    License version 2.1, as published by the Free Software Foundation.
- *
- *    This library is distributed in the hope that it will be useful,
- *    but WITHOUT ANY WARRANTY; without even the implied warranty of
- *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
- *    Lesser General Public License for more details.
- *
- *    You should have received a copy of the GNU Lesser General Public
- *    License along with this library; if not, write to the Free Software
- *    Foundation, Inc., 59 Temple Place, Suite 330, Boston,
- *    MA  02111-1307  USA
- *
- ************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
 
 // MARKER(update_precomp.py): autogen include statement, do not remove
 #include "precompiled_drawinglayer.hxx"
 
 #include <drawinglayer/attribute/sdrlineattribute.hxx>
Index: main/drawinglayer/source/attribute/fillgradientattribute.cxx
===================================================================
--- main/drawinglayer/source/attribute/fillgradientattribute.cxx	(revision 1232674)
+++ main/drawinglayer/source/attribute/fillgradientattribute.cxx	(revision 1239200)
@@ -1,39 +1,25 @@
-/*************************************************************************
- *
- *  OpenOffice.org - a multi-platform office productivity suite
- *
- *  $RCSfile: fillattribute.cxx,v $
- *
- *  $Revision: 1.4 $
- *
- *  last change: $Author: aw $ $Date: 2008-05-27 14:11:19 $
- *
- *  The Contents of this file are made available subject to
- *  the terms of GNU Lesser General Public License Version 2.1.
- *
- *
- *    GNU Lesser General Public License Version 2.1
- *    =============================================
- *    Copyright 2005 by Sun Microsystems, Inc.
- *    901 San Antonio Road, Palo Alto, CA 94303, USA
- *
- *    This library is free software; you can redistribute it and/or
- *    modify it under the terms of the GNU Lesser General Public
- *    License version 2.1, as published by the Free Software Foundation.
- *
- *    This library is distributed in the hope that it will be useful,
- *    but WITHOUT ANY WARRANTY; without even the implied warranty of
- *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
- *    Lesser General Public License for more details.
- *
- *    You should have received a copy of the GNU Lesser General Public
- *    License along with this library; if not, write to the Free Software
- *    Foundation, Inc., 59 Temple Place, Suite 330, Boston,
- *    MA  02111-1307  USA
- *
- ************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
 
 // MARKER(update_precomp.py): autogen include statement, do not remove
 #include "precompiled_drawinglayer.hxx"
 
 #include <drawinglayer/attribute/fillgradientattribute.hxx>
Index: main/drawinglayer/source/attribute/sdrlightingattribute3d.cxx
===================================================================
--- main/drawinglayer/source/attribute/sdrlightingattribute3d.cxx	(revision 1232674)
+++ main/drawinglayer/source/attribute/sdrlightingattribute3d.cxx	(revision 1239200)
@@ -1,39 +1,25 @@
-/*************************************************************************
- *
- *  OpenOffice.org - a multi-platform office productivity suite
- *
- *  $RCSfile: sdrattribute3d.cxx,v $
- *
- *  $Revision: 1.5 $
- *
- *  last change: $Author: aw $ $Date: 2008-05-27 14:11:19 $
- *
- *  The Contents of this file are made available subject to
- *  the terms of GNU Lesser General Public License Version 2.1.
- *
- *
- *    GNU Lesser General Public License Version 2.1
- *    =============================================
- *    Copyright 2005 by Sun Microsystems, Inc.
- *    901 San Antonio Road, Palo Alto, CA 94303, USA
- *
- *    This library is free software; you can redistribute it and/or
- *    modify it under the terms of the GNU Lesser General Public
- *    License version 2.1, as published by the Free Software Foundation.
- *
- *    This library is distributed in the hope that it will be useful,
- *    but WITHOUT ANY WARRANTY; without even the implied warranty of
- *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
- *    Lesser General Public License for more details.
- *
- *    You should have received a copy of the GNU Lesser General Public
- *    License along with this library; if not, write to the Free Software
- *    Foundation, Inc., 59 Temple Place, Suite 330, Boston,
- *    MA  02111-1307  USA
- *
- ************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
 
 // MARKER(update_precomp.py): autogen include statement, do not remove
 #include "precompiled_drawinglayer.hxx"
 
 #include <drawinglayer/attribute/sdrlightingattribute3d.hxx>
Index: main/drawinglayer/source/attribute/sdrlinestartendattribute.cxx
===================================================================
--- main/drawinglayer/source/attribute/sdrlinestartendattribute.cxx	(revision 1232674)
+++ main/drawinglayer/source/attribute/sdrlinestartendattribute.cxx	(revision 1239200)
@@ -1,39 +1,25 @@
-/*************************************************************************
- *
- *  OpenOffice.org - a multi-platform office productivity suite
- *
- *  $RCSfile: sdrattribute.cxx,v $
- *
- *  $Revision: 1.5 $
- *
- *  last change: $Author: aw $ $Date: 2008-05-27 14:11:19 $
- *
- *  The Contents of this file are made available subject to
- *  the terms of GNU Lesser General Public License Version 2.1.
- *
- *
- *    GNU Lesser General Public License Version 2.1
- *    =============================================
- *    Copyright 2005 by Sun Microsystems, Inc.
- *    901 San Antonio Road, Palo Alto, CA 94303, USA
- *
- *    This library is free software; you can redistribute it and/or
- *    modify it under the terms of the GNU Lesser General Public
- *    License version 2.1, as published by the Free Software Foundation.
- *
- *    This library is distributed in the hope that it will be useful,
- *    but WITHOUT ANY WARRANTY; without even the implied warranty of
- *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
- *    Lesser General Public License for more details.
- *
- *    You should have received a copy of the GNU Lesser General Public
- *    License along with this library; if not, write to the Free Software
- *    Foundation, Inc., 59 Temple Place, Suite 330, Boston,
- *    MA  02111-1307  USA
- *
- ************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
 
 // MARKER(update_precomp.py): autogen include statement, do not remove
 #include "precompiled_drawinglayer.hxx"
 
 #include <drawinglayer/attribute/sdrlinestartendattribute.hxx>
Index: main/drawinglayer/source/attribute/sdrobjectattribute3d.cxx
===================================================================
--- main/drawinglayer/source/attribute/sdrobjectattribute3d.cxx	(revision 1232674)
+++ main/drawinglayer/source/attribute/sdrobjectattribute3d.cxx	(revision 1239200)
@@ -1,39 +1,25 @@
-/*************************************************************************
- *
- *  OpenOffice.org - a multi-platform office productivity suite
- *
- *  $RCSfile: sdrattribute3d.cxx,v $
- *
- *  $Revision: 1.5 $
- *
- *  last change: $Author: aw $ $Date: 2008-05-27 14:11:19 $
- *
- *  The Contents of this file are made available subject to
- *  the terms of GNU Lesser General Public License Version 2.1.
- *
- *
- *    GNU Lesser General Public License Version 2.1
- *    =============================================
- *    Copyright 2005 by Sun Microsystems, Inc.
- *    901 San Antonio Road, Palo Alto, CA 94303, USA
- *
- *    This library is free software; you can redistribute it and/or
- *    modify it under the terms of the GNU Lesser General Public
- *    License version 2.1, as published by the Free Software Foundation.
- *
- *    This library is distributed in the hope that it will be useful,
- *    but WITHOUT ANY WARRANTY; without even the implied warranty of
- *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
- *    Lesser General Public License for more details.
- *
- *    You should have received a copy of the GNU Lesser General Public
- *    License along with this library; if not, write to the Free Software
- *    Foundation, Inc., 59 Temple Place, Suite 330, Boston,
- *    MA  02111-1307  USA
- *
- ************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
 
 // MARKER(update_precomp.py): autogen include statement, do not remove
 #include "precompiled_drawinglayer.hxx"
 
 #include <drawinglayer/attribute/sdrobjectattribute3d.hxx>
Index: main/drawinglayer/source/attribute/sdrlightattribute3d.cxx
===================================================================
--- main/drawinglayer/source/attribute/sdrlightattribute3d.cxx	(revision 1232674)
+++ main/drawinglayer/source/attribute/sdrlightattribute3d.cxx	(revision 1239200)
@@ -1,39 +1,25 @@
-/*************************************************************************
- *
- *  OpenOffice.org - a multi-platform office productivity suite
- *
- *  $RCSfile: sdrattribute3d.cxx,v $
- *
- *  $Revision: 1.5 $
- *
- *  last change: $Author: aw $ $Date: 2008-05-27 14:11:19 $
- *
- *  The Contents of this file are made available subject to
- *  the terms of GNU Lesser General Public License Version 2.1.
- *
- *
- *    GNU Lesser General Public License Version 2.1
- *    =============================================
- *    Copyright 2005 by Sun Microsystems, Inc.
- *    901 San Antonio Road, Palo Alto, CA 94303, USA
- *
- *    This library is free software; you can redistribute it and/or
- *    modify it under the terms of the GNU Lesser General Public
- *    License version 2.1, as published by the Free Software Foundation.
- *
- *    This library is distributed in the hope that it will be useful,
- *    but WITHOUT ANY WARRANTY; without even the implied warranty of
- *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
- *    Lesser General Public License for more details.
- *
- *    You should have received a copy of the GNU Lesser General Public
- *    License along with this library; if not, write to the Free Software
- *    Foundation, Inc., 59 Temple Place, Suite 330, Boston,
- *    MA  02111-1307  USA
- *
- ************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
 
 // MARKER(update_precomp.py): autogen include statement, do not remove
 #include "precompiled_drawinglayer.hxx"
 
 #include <drawinglayer/attribute/sdrlightattribute3d.hxx>
Index: main/drawinglayer/source/processor2d/vclmetafileprocessor2d.cxx
===================================================================
--- main/drawinglayer/source/processor2d/vclmetafileprocessor2d.cxx	(revision 1232674)
+++ main/drawinglayer/source/processor2d/vclmetafileprocessor2d.cxx	(revision 1239200)
@@ -1902,13 +1902,24 @@
 				            aViewRange.transform(maCurrentTransformation);
 		                    const Rectangle aRectLogic(
 			                    (sal_Int32)floor(aViewRange.getMinX()), (sal_Int32)floor(aViewRange.getMinY()), 
 			                    (sal_Int32)ceil(aViewRange.getMaxX()), (sal_Int32)ceil(aViewRange.getMaxY()));
 		                    const Rectangle aRectPixel(mpOutputDevice->LogicToPixel(aRectLogic));
-                            const Size aSizePixel(aRectPixel.GetSize());
+                            Size aSizePixel(aRectPixel.GetSize());
                     		const Point aEmptyPoint;
                             VirtualDevice aBufferDevice;
+                            const sal_uInt32 nMaxQuadratPixels(500000);
+                            const sal_uInt32 nViewVisibleArea(aSizePixel.getWidth() * aSizePixel.getHeight());
+                            double fReduceFactor(1.0);
+
+                            if(nViewVisibleArea > nMaxQuadratPixels)
+                            {
+                                // reduce render size
+                                fReduceFactor = sqrt((double)nMaxQuadratPixels / (double)nViewVisibleArea);
+                                aSizePixel = Size(basegfx::fround((double)aSizePixel.getWidth() * fReduceFactor),
+                                    basegfx::fround((double)aSizePixel.getHeight() * fReduceFactor));
+                            }
 
                             if(aBufferDevice.SetOutputSizePixel(aSizePixel))
                             {
                                 // create and set MapModes for target devices
 		                        MapMode aNewMapMode(mpOutputDevice->GetMapMode());
@@ -1928,10 +1939,16 @@
                                 if(!basegfx::fTools::equal(fDPIXChange, 1.0) || !basegfx::fTools::equal(fDPIYChange, 1.0))
                                 {
                                     aViewTransform.scale(fDPIXChange, fDPIYChange);
                                 }
 
+                                // also take scaling from Size reduction into acount
+                                if(!basegfx::fTools::equal(fReduceFactor, 1.0))
+                                {
+                                    aViewTransform.scale(fReduceFactor, fReduceFactor);
+                                }
+
                                 // create view information and pixel renderer. Reuse known ViewInformation
 								// except new transformation and range
                                 const geometry::ViewInformation2D aViewInfo(
 									getViewInformation2D().getObjectTransformation(),
 									aViewTransform, 
Index: main/drawinglayer/source/processor2d/vclprocessor2d.cxx
===================================================================
--- main/drawinglayer/source/processor2d/vclprocessor2d.cxx	(revision 1232674)
+++ main/drawinglayer/source/processor2d/vclprocessor2d.cxx	(revision 1239200)
@@ -54,10 +54,13 @@
 #include <drawinglayer/primitive2d/pagepreviewprimitive2d.hxx>
 #include <tools/diagnose_ex.h>
 #include <vcl/metric.hxx>
 #include <drawinglayer/primitive2d/textenumsprimitive2d.hxx>
 #include <drawinglayer/primitive2d/epsprimitive2d.hxx>
+#include <drawinglayer/primitive2d/svggradientprimitive2d.hxx>
+#include <basegfx/color/bcolor.hxx>
+#include <basegfx/matrix/b2dhommatrixtools.hxx>
 
 //////////////////////////////////////////////////////////////////////////////
 // control support
 
 #include <com/sun/star/awt/XWindow2.hpp>
@@ -76,10 +79,38 @@
 
 using namespace com::sun::star;
 
 //////////////////////////////////////////////////////////////////////////////
 
+namespace
+{
+    sal_uInt32 calculateStepsForSvgGradient(const basegfx::BColor& rColorA, const basegfx::BColor& rColorB, double fDelta, double fDiscreteUnit)
+    {
+        // use color distance, assume to do every color step
+        sal_uInt32 nSteps(basegfx::fround(rColorA.getDistance(rColorB) * 255.0));
+
+        if(nSteps)
+        {
+            // calc discrete length to change color each disctete unit (pixel)
+            const sal_uInt32 nDistSteps(basegfx::fround(fDelta / fDiscreteUnit));
+
+            nSteps = std::min(nSteps, nDistSteps);
+        }
+
+        // reduce quality to 3 discrete units or every 3rd color step for rendering
+        nSteps /= 2;
+
+        // roughly cut when too big or too small (not full quality, reduce complexity)
+        nSteps = std::min(nSteps, sal_uInt32(255));
+        nSteps = std::max(nSteps, sal_uInt32(1));
+
+        return nSteps;
+    }
+} // end of anonymous namespace
+
+//////////////////////////////////////////////////////////////////////////////
+
 namespace drawinglayer
 {
 	namespace processor2d
 	{
 		//////////////////////////////////////////////////////////////////////////////
@@ -1329,10 +1360,104 @@
                     }
                 }
             }
         }
 
+        void VclProcessor2D::RenderSvgLinearAtomPrimitive2D(const primitive2d::SvgLinearAtomPrimitive2D& rCandidate)
+        {
+            const double fDelta(rCandidate.getOffsetB() - rCandidate.getOffsetA());
+
+            if(basegfx::fTools::more(fDelta, 0.0))
+            {
+                const basegfx::BColor aColorA(maBColorModifierStack.getModifiedColor(rCandidate.getColorA()));
+                const basegfx::BColor aColorB(maBColorModifierStack.getModifiedColor(rCandidate.getColorB()));
+                const double fDiscreteUnit((getViewInformation2D().getInverseObjectToViewTransformation() * basegfx::B2DVector(1.0, 0.0)).getLength());
+
+                // use color distance and discrete lengths to calculate step count
+                const sal_uInt32 nSteps(calculateStepsForSvgGradient(aColorA, aColorB, fDelta, fDiscreteUnit));
+
+                // prepare loop and polygon
+                double fStart(0.0);
+                double fStep(fDelta / nSteps);
+                const basegfx::B2DPolygon aPolygon(
+                    basegfx::tools::createPolygonFromRect(
+                        basegfx::B2DRange(
+                            rCandidate.getOffsetA() - fDiscreteUnit, 
+                            0.0, 
+                            rCandidate.getOffsetA() + fStep + fDiscreteUnit, 
+                            1.0)));
+
+                // switch off line painting
+                mpOutputDevice->SetLineColor();
+
+                // loop and paint
+                for(sal_uInt32 a(0); a < nSteps; a++, fStart += fStep)
+                {
+                    basegfx::B2DPolygon aNew(aPolygon);
+
+                    aNew.transform(maCurrentTransformation * basegfx::tools::createTranslateB2DHomMatrix(fStart, 0.0));
+                    mpOutputDevice->SetFillColor(Color(basegfx::interpolate(aColorA, aColorB, fStart/fDelta)));
+                    mpOutputDevice->DrawPolyPolygon(basegfx::B2DPolyPolygon(aNew));
+                }
+            }
+        }
+
+        void VclProcessor2D::RenderSvgRadialAtomPrimitive2D(const primitive2d::SvgRadialAtomPrimitive2D& rCandidate)
+        {
+            const double fDeltaScale(rCandidate.getScaleB() - rCandidate.getScaleA());
+
+            if(basegfx::fTools::more(fDeltaScale, 0.0))
+            {
+                const basegfx::BColor aColorA(maBColorModifierStack.getModifiedColor(rCandidate.getColorA()));
+                const basegfx::BColor aColorB(maBColorModifierStack.getModifiedColor(rCandidate.getColorB()));
+                const double fDiscreteUnit((getViewInformation2D().getInverseObjectToViewTransformation() * basegfx::B2DVector(1.0, 0.0)).getLength());
+
+                // use color distance and discrete lengths to calculate step count
+                const sal_uInt32 nSteps(calculateStepsForSvgGradient(aColorA, aColorB, fDeltaScale, fDiscreteUnit));
+
+                // switch off line painting
+                mpOutputDevice->SetLineColor();
+
+                // prepare loop (outside to inside)
+                double fEndScale(rCandidate.getScaleB());
+                double fStepScale(fDeltaScale / nSteps);
+
+                for(sal_uInt32 a(0); a < nSteps; a++, fEndScale -= fStepScale)
+                {
+                    const double fUnitScale(fEndScale/fDeltaScale);
+                    basegfx::B2DHomMatrix aTransform;
+
+                    if(rCandidate.isTranslateSet())
+                    {
+                        const basegfx::B2DVector aTranslate(
+                            basegfx::interpolate(
+                                rCandidate.getTranslateA(), 
+                                rCandidate.getTranslateB(), 
+                                fUnitScale));
+
+                        aTransform = basegfx::tools::createScaleTranslateB2DHomMatrix(
+                            fEndScale,
+                            fEndScale,
+                            aTranslate.getX(),
+                            aTranslate.getY());
+                    }
+                    else
+                    {
+                        aTransform = basegfx::tools::createScaleB2DHomMatrix(
+                            fEndScale,
+                            fEndScale);
+                    }
+
+                    basegfx::B2DPolygon aNew(basegfx::tools::createPolygonFromUnitCircle());
+                    
+                    aNew.transform(maCurrentTransformation * aTransform);
+                    mpOutputDevice->SetFillColor(Color(basegfx::interpolate(aColorA, aColorB, fUnitScale)));
+                    mpOutputDevice->DrawPolyPolygon(basegfx::B2DPolyPolygon(aNew));
+                }
+            }
+        }
+
 		void VclProcessor2D::adaptLineToFillDrawMode() const
 		{
 			const sal_uInt32 nOriginalDrawMode(mpOutputDevice->GetDrawMode());
 
 			if(nOriginalDrawMode & (DRAWMODE_BLACKLINE|DRAWMODE_GRAYLINE|DRAWMODE_GHOSTEDLINE|DRAWMODE_WHITELINE|DRAWMODE_SETTINGSLINE))
Index: main/drawinglayer/source/processor2d/vclpixelprocessor2d.cxx
===================================================================
--- main/drawinglayer/source/processor2d/vclpixelprocessor2d.cxx	(revision 1232674)
+++ main/drawinglayer/source/processor2d/vclpixelprocessor2d.cxx	(revision 1239200)
@@ -53,11 +53,11 @@
 #include <drawinglayer/primitive2d/invertprimitive2d.hxx>
 #include <cstdio>
 #include <drawinglayer/primitive2d/backgroundcolorprimitive2d.hxx>
 #include <basegfx/matrix/b2dhommatrixtools.hxx>
 #include <drawinglayer/primitive2d/epsprimitive2d.hxx>
-
+#include <drawinglayer/primitive2d/svggradientprimitive2d.hxx>
 #include <toolkit/helper/vclunohelper.hxx>
 #include <vcl/window.hxx>
 
 //////////////////////////////////////////////////////////////////////////////
 
@@ -571,10 +571,20 @@
                 case PRIMITIVE2D_ID_EPSPRIMITIVE2D :
                 {
 					RenderEpsPrimitive2D(static_cast< const primitive2d::EpsPrimitive2D& >(rCandidate));
                     break;
                 }
+                case PRIMITIVE2D_ID_SVGLINEARATOMPRIMITIVE2D:
+                {
+                    RenderSvgLinearAtomPrimitive2D(static_cast< const primitive2d::SvgLinearAtomPrimitive2D& >(rCandidate));
+                    break;
+                }
+                case PRIMITIVE2D_ID_SVGRADIALATOMPRIMITIVE2D:
+                {
+                    RenderSvgRadialAtomPrimitive2D(static_cast< const primitive2d::SvgRadialAtomPrimitive2D& >(rCandidate));
+                    break;
+                }
 				default :
 				{
 					// process recursively
 					process(rCandidate.get2DDecomposition(getViewInformation2D()));
 					break;
Index: main/drawinglayer/source/primitive2d/sdrdecompositiontools2d.cxx
===================================================================
--- main/drawinglayer/source/primitive2d/sdrdecompositiontools2d.cxx	(revision 1232674)
+++ main/drawinglayer/source/primitive2d/sdrdecompositiontools2d.cxx	(revision 1239200)
@@ -1,39 +1,25 @@
-/*************************************************************************
- *
- *  OpenOffice.org - a multi-platform office productivity suite
- *
- *  $RCSfile: sdrdecompositiontools3d.cxx,v $
- *
- *  $Revision: 1.7 $
- *
- *  last change: $Author: aw $ $Date: 2008-05-27 14:11:21 $
- *
- *  The Contents of this file are made available subject to
- *  the terms of GNU Lesser General Public License Version 2.1.
- *
- *
- *    GNU Lesser General Public License Version 2.1
- *    =============================================
- *    Copyright 2005 by Sun Microsystems, Inc.
- *    901 San Antonio Road, Palo Alto, CA 94303, USA
- *
- *    This library is free software; you can redistribute it and/or
- *    modify it under the terms of the GNU Lesser General Public
- *    License version 2.1, as published by the Free Software Foundation.
- *
- *    This library is distributed in the hope that it will be useful,
- *    but WITHOUT ANY WARRANTY; without even the implied warranty of
- *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
- *    Lesser General Public License for more details.
- *
- *    You should have received a copy of the GNU Lesser General Public
- *    License along with this library; if not, write to the Free Software
- *    Foundation, Inc., 59 Temple Place, Suite 330, Boston,
- *    MA  02111-1307  USA
- *
- ************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
 
 // MARKER(update_precomp.py): autogen include statement, do not remove
 #include "precompiled_drawinglayer.hxx"
 
 #include <drawinglayer/primitive2d/sdrdecompositiontools2d.hxx>
Index: main/drawinglayer/source/primitive2d/svggradientprimitive2d.cxx
===================================================================
--- main/drawinglayer/source/primitive2d/svggradientprimitive2d.cxx	(revision 1232674)
+++ main/drawinglayer/source/primitive2d/svggradientprimitive2d.cxx	(revision 1239200)
@@ -39,16 +39,41 @@
 
 using namespace com::sun::star;
 
 //////////////////////////////////////////////////////////////////////////////
 
+namespace
+{
+    sal_uInt32 calculateStepsForSvgGradient(const basegfx::BColor& rColorA, const basegfx::BColor& rColorB, double fDelta, double fDiscreteUnit)
+    {
+        // use color distance, assume to do every color step (full quality)
+        sal_uInt32 nSteps(basegfx::fround(rColorA.getDistance(rColorB) * 255.0));
+
+        if(nSteps)
+        {
+            // calc discrete length to change color all 1.5 disctete units (pixels)
+            const sal_uInt32 nDistSteps(basegfx::fround(fDelta / (fDiscreteUnit * 1.5)));
+
+            nSteps = std::min(nSteps, nDistSteps);
+        }
+
+        // roughly cut when too big or too small
+        nSteps = std::min(nSteps, sal_uInt32(255));
+        nSteps = std::max(nSteps, sal_uInt32(1));
+
+        return nSteps;
+    }
+} // end of anonymous namespace
+
+//////////////////////////////////////////////////////////////////////////////
+
 namespace drawinglayer
 {
-	namespace primitive2d
-	{
-		Primitive2DSequence SvgGradientHelper::createSingleGradientEntryFill() const
-		{
+    namespace primitive2d
+    {
+        Primitive2DSequence SvgGradientHelper::createSingleGradientEntryFill() const
+        {
             const SvgGradientEntryVector& rEntries = getGradientEntries();
             const sal_uInt32 nCount(rEntries.size());
             Primitive2DSequence xRetval;
 
             if(nCount)
@@ -56,19 +81,19 @@
                 const SvgGradientEntry& rSingleEntry = rEntries[nCount - 1];
                 const double fOpacity(rSingleEntry.getOpacity());
 
                 if(fOpacity > 0.0)
                 {
-    			    Primitive2DReference xRef(
+                    Primitive2DReference xRef(
                         new PolyPolygonColorPrimitive2D(
                             getPolyPolygon(), 
                             rSingleEntry.getColor()));
 
                     if(fOpacity < 1.0)
                     {
-            		    const Primitive2DSequence aContent(&xRef, 1);
-    				    
+                        const Primitive2DSequence aContent(&xRef, 1);
+
                         xRef = Primitive2DReference(
                             new UnifiedTransparencePrimitive2D(
                                 aContent, 
                                 1.0 - fOpacity));
                     }
@@ -228,15 +253,15 @@
             const Primitive2DSequence aTargetColorEntries(Primitive2DVectorToPrimitive2DSequence(rTargetColor, bInvert));
             const Primitive2DSequence aTargetOpacityEntries(Primitive2DVectorToPrimitive2DSequence(rTargetOpacity, bInvert));
 
             if(aTargetColorEntries.hasElements())
             {
-    			Primitive2DReference xRefContent;
+                Primitive2DReference xRefContent;
                     
                 if(aTargetOpacityEntries.hasElements())
                 {
-        			const Primitive2DReference xRefOpacity = new TransparencePrimitive2D(
+                    const Primitive2DReference xRefOpacity = new TransparencePrimitive2D(
                         aTargetColorEntries, 
                         aTargetOpacityEntries);
                         
                     xRefContent = new TransformPrimitive2D(
                         rUnitGradientToObject, 
@@ -257,48 +282,45 @@
             }
 
             return xRetval;
         }
 
-		SvgGradientHelper::SvgGradientHelper(
-			const basegfx::B2DPolyPolygon& rPolyPolygon,
+        SvgGradientHelper::SvgGradientHelper(
+            const basegfx::B2DPolyPolygon& rPolyPolygon,
             const SvgGradientEntryVector& rGradientEntries,
             const basegfx::B2DPoint& rStart,
-            SpreadMethod aSpreadMethod,
-            double fOverlapping)
-		:	maPolyPolygon(rPolyPolygon),
+            SpreadMethod aSpreadMethod)
+        :   maPolyPolygon(rPolyPolygon),
             maGradientEntries(rGradientEntries),
             maStart(rStart),
             maSpreadMethod(aSpreadMethod),
-            mfOverlapping(fOverlapping),
             mbPreconditionsChecked(false),
             mbCreatesContent(false),
             mbSingleEntry(false),
             mbFullyOpaque(true)
-		{
-		}
+        {
+        }
 
-		bool SvgGradientHelper::operator==(const SvgGradientHelper& rSvgGradientHelper) const
-		{
-			const SvgGradientHelper& rCompare = static_cast< const SvgGradientHelper& >(rSvgGradientHelper);
+        bool SvgGradientHelper::operator==(const SvgGradientHelper& rSvgGradientHelper) const
+        {
+            const SvgGradientHelper& rCompare = static_cast< const SvgGradientHelper& >(rSvgGradientHelper);
 
-			return (getPolyPolygon() == rCompare.getPolyPolygon()
+            return (getPolyPolygon() == rCompare.getPolyPolygon()
                 && getGradientEntries() == rCompare.getGradientEntries()
                 && getStart() == rCompare.getStart()
-                && getSpreadMethod() == rCompare.getSpreadMethod()
-                && getOverlapping() == rCompare.getOverlapping());
-		}
+                && getSpreadMethod() == rCompare.getSpreadMethod());
+        }
 
     } // end of namespace primitive2d
 } // end of namespace drawinglayer
 
 //////////////////////////////////////////////////////////////////////////////
 
 namespace drawinglayer
 {
 	namespace primitive2d
-	{
+    {
         void SvgLinearGradientPrimitive2D::checkPreconditions()
         {
             // call parent
             SvgGradientHelper::checkPreconditions();
 
@@ -313,28 +335,10 @@
                     setSingleEntry();
                 }
             }
         }
 
-        void SvgLinearGradientPrimitive2D::ensureGeometry(
-            basegfx::B2DPolyPolygon& rPolyPolygon,
-            const SvgGradientEntry& rFrom, 
-            const SvgGradientEntry& rTo,
-            sal_Int32 nOffset) const
-        {
-            if(!rPolyPolygon.count())
-            {
-                rPolyPolygon.append(
-                    basegfx::tools::createPolygonFromRect(
-                        basegfx::B2DRange(
-                            rFrom.getOffset() - getOverlapping() + nOffset, 
-                            0.0, 
-                            rTo.getOffset() + getOverlapping() + nOffset, 
-                            1.0)));
-            }
-        }
-
         void SvgLinearGradientPrimitive2D::createAtom(
             Primitive2DVector& rTargetColor,
             Primitive2DVector& rTargetOpacity, 
             const SvgGradientEntry& rFrom, 
             const SvgGradientEntry& rTo,
@@ -345,57 +349,27 @@
             {
                 OSL_ENSURE(false, "SvgGradient Atom creation with no step width (!)");
             }
             else
             {
-                const bool bColorChange(rFrom.getColor() != rTo.getColor());
-                const bool bOpacityChange(rFrom.getOpacity() != rTo.getOpacity());
-                basegfx::B2DPolyPolygon aPolyPolygon;
-                                            
-                if(bColorChange)
-                {
-    		        rTargetColor.push_back(
-                        new SvgLinearAtomPrimitive2D(
-                            rFrom.getColor(), rFrom.getOffset() + nOffset,
-                            rTo.getColor(), rTo.getOffset() + nOffset,
-                            getOverlapping()));
-                }
-                else
-                {
-                    ensureGeometry(aPolyPolygon, rFrom, rTo, nOffset);
-                    rTargetColor.push_back(
-                        new PolyPolygonColorPrimitive2D(
-                            aPolyPolygon, 
-                            rFrom.getColor()));
-                }
-
-                if(bOpacityChange)
-                {
-                    const double fTransFrom(1.0 - rFrom.getOpacity());
-                    const double fTransTo(1.0 - rTo.getOpacity());
-
-                    rTargetOpacity.push_back(
-                        new SvgLinearAtomPrimitive2D(
-                            basegfx::BColor(fTransFrom, fTransFrom, fTransFrom), rFrom.getOffset() + nOffset,
-                            basegfx::BColor(fTransTo,fTransTo, fTransTo), rTo.getOffset() + nOffset,
-                            getOverlapping()));
-                }
-                else if(!getFullyOpaque())
-                {
-                    const double fTransparence(1.0 - rFrom.getOpacity());
-
-                    ensureGeometry(aPolyPolygon, rFrom, rTo, nOffset);
-                    rTargetOpacity.push_back(
-                        new PolyPolygonColorPrimitive2D(
-                            aPolyPolygon, 
-                            basegfx::BColor(fTransparence, fTransparence, fTransparence)));
-                }
+                rTargetColor.push_back(
+                    new SvgLinearAtomPrimitive2D(
+                        rFrom.getColor(), rFrom.getOffset() + nOffset,
+                        rTo.getColor(), rTo.getOffset() + nOffset));
+
+                const double fTransFrom(1.0 - rFrom.getOpacity());
+                const double fTransTo(1.0 - rTo.getOpacity());
+
+                rTargetOpacity.push_back(
+                    new SvgLinearAtomPrimitive2D(
+                        basegfx::BColor(fTransFrom, fTransFrom, fTransFrom), rFrom.getOffset() + nOffset,
+                        basegfx::BColor(fTransTo,fTransTo, fTransTo), rTo.getOffset() + nOffset));
             }
         }
 
-		Primitive2DSequence SvgLinearGradientPrimitive2D::create2DDecomposition(const geometry::ViewInformation2D& /*rViewInformation*/) const
-		{
+        Primitive2DSequence SvgLinearGradientPrimitive2D::create2DDecomposition(const geometry::ViewInformation2D& /*rViewInformation*/) const
+        {
             Primitive2DSequence xRetval;
 
             if(!getPreconditionsChecked())
             {
                 const_cast< SvgLinearGradientPrimitive2D* >(this)->checkPreconditions();
@@ -547,57 +521,56 @@
 
                 xRetval = createResult(aTargetColor, aTargetOpacity, aUnitGradientToObject);
             }
 
             return xRetval;
-		}
+        }
 
-		SvgLinearGradientPrimitive2D::SvgLinearGradientPrimitive2D(
-			const basegfx::B2DPolyPolygon& rPolyPolygon,
+        SvgLinearGradientPrimitive2D::SvgLinearGradientPrimitive2D(
+            const basegfx::B2DPolyPolygon& rPolyPolygon,
             const SvgGradientEntryVector& rGradientEntries,
             const basegfx::B2DPoint& rStart,
             const basegfx::B2DPoint& rEnd,
-            SpreadMethod aSpreadMethod,
-            double fOverlapping)
-		:	BufferedDecompositionPrimitive2D(),
-            SvgGradientHelper(rPolyPolygon, rGradientEntries, rStart, aSpreadMethod, fOverlapping),
+            SpreadMethod aSpreadMethod)
+        :   BufferedDecompositionPrimitive2D(),
+            SvgGradientHelper(rPolyPolygon, rGradientEntries, rStart, aSpreadMethod),
             maEnd(rEnd)
-		{
-		}
+        {
+        }
 
-		bool SvgLinearGradientPrimitive2D::operator==(const BasePrimitive2D& rPrimitive) const
-		{
+        bool SvgLinearGradientPrimitive2D::operator==(const BasePrimitive2D& rPrimitive) const
+        {
             const SvgGradientHelper* pSvgGradientHelper = dynamic_cast< const SvgGradientHelper* >(&rPrimitive);
 
             if(pSvgGradientHelper && SvgGradientHelper::operator==(*pSvgGradientHelper))
-			{
-				const SvgLinearGradientPrimitive2D& rCompare = static_cast< const SvgLinearGradientPrimitive2D& >(rPrimitive);
+            {
+                const SvgLinearGradientPrimitive2D& rCompare = static_cast< const SvgLinearGradientPrimitive2D& >(rPrimitive);
 
-				return (getEnd() == rCompare.getEnd());
-			}
+                return (getEnd() == rCompare.getEnd());
+            }
 
-			return false;
-		}
+            return false;
+        }
 
-		basegfx::B2DRange SvgLinearGradientPrimitive2D::getB2DRange(const geometry::ViewInformation2D& /*rViewInformation*/) const
-		{
-			// return ObjectRange
-			return getPolyPolygon().getB2DRange();
-		}
+        basegfx::B2DRange SvgLinearGradientPrimitive2D::getB2DRange(const geometry::ViewInformation2D& /*rViewInformation*/) const
+        {
+            // return ObjectRange
+            return getPolyPolygon().getB2DRange();
+        }
 
-		// provide unique ID
-		ImplPrimitrive2DIDBlock(SvgLinearGradientPrimitive2D, PRIMITIVE2D_ID_SVGLINEARGRADIENTPRIMITIVE2D)
+        // provide unique ID
+        ImplPrimitrive2DIDBlock(SvgLinearGradientPrimitive2D, PRIMITIVE2D_ID_SVGLINEARGRADIENTPRIMITIVE2D)
 
-	} // end of namespace primitive2d
+    } // end of namespace primitive2d
 } // end of namespace drawinglayer
 
 //////////////////////////////////////////////////////////////////////////////
 
 namespace drawinglayer
 {
-	namespace primitive2d
-	{
+    namespace primitive2d
+    {
         void SvgRadialGradientPrimitive2D::checkPreconditions()
         {
             // call parent
             SvgGradientHelper::checkPreconditions();
 
@@ -610,64 +583,10 @@
                     setSingleEntry();
                 }
             }
         }
 
-        void SvgRadialGradientPrimitive2D::ensureGeometry(
-            basegfx::B2DPolyPolygon& rPolyPolygon,
-            const SvgGradientEntry& rFrom, 
-            const SvgGradientEntry& rTo,
-            sal_Int32 nOffset) const
-        {
-            if(!rPolyPolygon.count())
-            {
-                basegfx::B2DPolygon aPolygonA(basegfx::tools::createPolygonFromUnitCircle());
-                basegfx::B2DPolygon aPolygonB(basegfx::tools::createPolygonFromUnitCircle());
-                double fScaleFrom(rFrom.getOffset() + nOffset);
-                const double fScaleTo(rTo.getOffset() + nOffset);
-
-                if(fScaleFrom > getOverlapping())
-                {
-                    fScaleFrom -= getOverlapping();
-                }
-
-                if(isFocalSet())
-                {
-                    const basegfx::B2DVector aTranslateFrom(maFocalVector * (maFocalLength - fScaleFrom));
-                    const basegfx::B2DVector aTranslateTo(maFocalVector * (maFocalLength - fScaleTo));
-
-                    aPolygonA.transform(
-                        basegfx::tools::createScaleTranslateB2DHomMatrix(
-                            fScaleFrom,
-                            fScaleFrom,
-                            aTranslateFrom.getX(),
-                            aTranslateFrom.getY()));
-                    aPolygonB.transform(
-                        basegfx::tools::createScaleTranslateB2DHomMatrix(
-                            fScaleTo,
-                            fScaleTo,
-                            aTranslateTo.getX(),
-                            aTranslateTo.getY()));
-                }
-                else
-                {
-                    aPolygonA.transform(
-                        basegfx::tools::createScaleB2DHomMatrix(
-                            fScaleFrom,
-                            fScaleFrom));
-                    aPolygonB.transform(
-                        basegfx::tools::createScaleB2DHomMatrix(
-                            fScaleTo,
-                            fScaleTo));
-                }
-
-                // add the outer polygon first
-                rPolyPolygon.append(aPolygonB);
-                rPolyPolygon.append(aPolygonA);
-            }
-        }
-
         void SvgRadialGradientPrimitive2D::createAtom(
             Primitive2DVector& rTargetColor,
             Primitive2DVector& rTargetOpacity, 
             const SvgGradientEntry& rFrom, 
             const SvgGradientEntry& rTo,
@@ -678,86 +597,52 @@
             {
                 OSL_ENSURE(false, "SvgGradient Atom creation with no step width (!)");
             }
             else
             {
-                const bool bColorChange(rFrom.getColor() != rTo.getColor());
-                const bool bOpacityChange(rFrom.getOpacity() != rTo.getOpacity());
-                basegfx::B2DPolyPolygon aPolyPolygon;
+                const double fScaleFrom(rFrom.getOffset() + nOffset);
+                const double fScaleTo(rTo.getOffset() + nOffset);
 
-                if(bColorChange)
+                if(isFocalSet())
                 {
-                    const double fScaleFrom(rFrom.getOffset() + nOffset);
-                    const double fScaleTo(rTo.getOffset() + nOffset);
-
-                    if(isFocalSet())
-                    {
-                        const basegfx::B2DVector aTranslateFrom(maFocalVector * (maFocalLength - fScaleFrom));
-                        const basegfx::B2DVector aTranslateTo(maFocalVector * (maFocalLength - fScaleTo));
+                    const basegfx::B2DVector aTranslateFrom(maFocalVector * (maFocalLength - fScaleFrom));
+                    const basegfx::B2DVector aTranslateTo(maFocalVector * (maFocalLength - fScaleTo));
                         
-                        rTargetColor.push_back(
-                            new SvgRadialAtomPrimitive2D(
-                                rFrom.getColor(), fScaleFrom, aTranslateFrom,
-                                rTo.getColor(), fScaleTo, aTranslateTo,
-                                getOverlapping()));
-                    }
-                    else
-                    {
-                        rTargetColor.push_back(
-                            new SvgRadialAtomPrimitive2D(
-                                rFrom.getColor(), fScaleFrom,
-                                rTo.getColor(), fScaleTo,
-                                getOverlapping()));
-                    }
+                    rTargetColor.push_back(
+                        new SvgRadialAtomPrimitive2D(
+                            rFrom.getColor(), fScaleFrom, aTranslateFrom,
+                            rTo.getColor(), fScaleTo, aTranslateTo));
                 }
                 else
                 {
-                    ensureGeometry(aPolyPolygon, rFrom, rTo, nOffset);
                     rTargetColor.push_back(
-                        new PolyPolygonColorPrimitive2D(
-                            aPolyPolygon, 
-                            rFrom.getColor()));
+                        new SvgRadialAtomPrimitive2D(
+                            rFrom.getColor(), fScaleFrom,
+                            rTo.getColor(), fScaleTo));
                 }
 
-                if(bOpacityChange)
-                {
-                    const double fTransFrom(1.0 - rFrom.getOpacity());
-                    const double fTransTo(1.0 - rTo.getOpacity());
-                    const basegfx::BColor aColorFrom(fTransFrom, fTransFrom, fTransFrom);
-                    const basegfx::BColor aColorTo(fTransTo, fTransTo, fTransTo);
-                    const double fScaleFrom(rFrom.getOffset() + nOffset);
-                    const double fScaleTo(rTo.getOffset() + nOffset);
+                const double fTransFrom(1.0 - rFrom.getOpacity());
+                const double fTransTo(1.0 - rTo.getOpacity());
+                const basegfx::BColor aColorFrom(fTransFrom, fTransFrom, fTransFrom);
+                const basegfx::BColor aColorTo(fTransTo, fTransTo, fTransTo);
 
-                    if(isFocalSet())
-                    {
-                        const basegfx::B2DVector aTranslateFrom(maFocalVector * (maFocalLength - fScaleFrom));
-                        const basegfx::B2DVector aTranslateTo(maFocalVector * (maFocalLength - fScaleTo));
+                if(isFocalSet())
+                {
+                    const basegfx::B2DVector aTranslateFrom(maFocalVector * (maFocalLength - fScaleFrom));
+                    const basegfx::B2DVector aTranslateTo(maFocalVector * (maFocalLength - fScaleTo));
                         
-                        rTargetOpacity.push_back(
-                            new SvgRadialAtomPrimitive2D(
-                                aColorFrom, fScaleFrom, aTranslateFrom,
-                                aColorTo, fScaleTo, aTranslateTo,
-                                getOverlapping()));
-                    }
-                    else
-                    {
-                        rTargetOpacity.push_back(
-                            new SvgRadialAtomPrimitive2D(
-                                aColorFrom, fScaleFrom,
-                                aColorTo, fScaleTo,
-                                getOverlapping()));
-                    }
+                    rTargetOpacity.push_back(
+                        new SvgRadialAtomPrimitive2D(
+                            aColorFrom, fScaleFrom, aTranslateFrom,
+                            aColorTo, fScaleTo, aTranslateTo));
                 }
-                else if(!getFullyOpaque())
+                else
                 {
-                    const double fTransparence(1.0 - rFrom.getOpacity());
-
-                    ensureGeometry(aPolyPolygon, rFrom, rTo, nOffset);
                     rTargetOpacity.push_back(
-                        new PolyPolygonColorPrimitive2D(
-                            aPolyPolygon, 
-                            basegfx::BColor(fTransparence, fTransparence, fTransparence)));
+                        new SvgRadialAtomPrimitive2D(
+                            aColorFrom, fScaleFrom,
+                            aColorTo, fScaleTo));
                 }
             }
         }
 
         const SvgGradientEntryVector& SvgRadialGradientPrimitive2D::getMirroredGradientEntries() const
@@ -789,12 +674,12 @@
                             rCandidate.getOpacity()));
                 }
             }
         }
 
-		Primitive2DSequence SvgRadialGradientPrimitive2D::create2DDecomposition(const geometry::ViewInformation2D& /*rViewInformation*/) const
-		{
+        Primitive2DSequence SvgRadialGradientPrimitive2D::create2DDecomposition(const geometry::ViewInformation2D& /*rViewInformation*/) const
+        {
             Primitive2DSequence xRetval;
 
             if(!getPreconditionsChecked())
             {
                 const_cast< SvgRadialGradientPrimitive2D* >(this)->checkPreconditions();
@@ -891,46 +776,45 @@
 
                 xRetval = createResult(aTargetColor, aTargetOpacity, aUnitGradientToObject, true);
             }
 
             return xRetval;
-		}
+        }
 
-		SvgRadialGradientPrimitive2D::SvgRadialGradientPrimitive2D(
-			const basegfx::B2DPolyPolygon& rPolyPolygon,
+        SvgRadialGradientPrimitive2D::SvgRadialGradientPrimitive2D(
+            const basegfx::B2DPolyPolygon& rPolyPolygon,
             const SvgGradientEntryVector& rGradientEntries,
             const basegfx::B2DPoint& rStart,
             double fRadius,
             SpreadMethod aSpreadMethod,
-            const basegfx::B2DPoint* pFocal,
-            double fOverlapping)
-		:	BufferedDecompositionPrimitive2D(),
-            SvgGradientHelper(rPolyPolygon, rGradientEntries, rStart, aSpreadMethod, fOverlapping),
+            const basegfx::B2DPoint* pFocal)
+        :   BufferedDecompositionPrimitive2D(),
+            SvgGradientHelper(rPolyPolygon, rGradientEntries, rStart, aSpreadMethod),
             mfRadius(fRadius),
             maFocal(rStart),
             maFocalVector(0.0, 0.0),
             maFocalLength(0.0),
             maMirroredGradientEntries(),
             mbFocalSet(false)
-		{
+        {
             if(pFocal)
             {
                 maFocal = *pFocal;
                 maFocalVector = maFocal - getStart();
                 mbFocalSet = true;
             }
-		}
+        }
 
-		bool SvgRadialGradientPrimitive2D::operator==(const BasePrimitive2D& rPrimitive) const
-		{
+        bool SvgRadialGradientPrimitive2D::operator==(const BasePrimitive2D& rPrimitive) const
+        {
             const SvgGradientHelper* pSvgGradientHelper = dynamic_cast< const SvgGradientHelper* >(&rPrimitive);
 
             if(pSvgGradientHelper && SvgGradientHelper::operator==(*pSvgGradientHelper))
-			{
-				const SvgRadialGradientPrimitive2D& rCompare = static_cast< const SvgRadialGradientPrimitive2D& >(rPrimitive);
+            {
+                const SvgRadialGradientPrimitive2D& rCompare = static_cast< const SvgRadialGradientPrimitive2D& >(rPrimitive);
 
-				if(getRadius() == rCompare.getRadius())
+                if(getRadius() == rCompare.getRadius())
                 {
                     if(isFocalSet() == rCompare.isFocalSet())
                     {
                         if(isFocalSet())
                         {
@@ -940,309 +824,270 @@
                         {
                             return true;
                         }
                     }
                 }
-			}
+            }
 
-			return false;
-		}
+            return false;
+        }
 
-		basegfx::B2DRange SvgRadialGradientPrimitive2D::getB2DRange(const geometry::ViewInformation2D& /*rViewInformation*/) const
-		{
-			// return ObjectRange
-			return getPolyPolygon().getB2DRange();
-		}
+        basegfx::B2DRange SvgRadialGradientPrimitive2D::getB2DRange(const geometry::ViewInformation2D& /*rViewInformation*/) const
+        {
+            // return ObjectRange
+            return getPolyPolygon().getB2DRange();
+        }
 
-		// provide unique ID
-		ImplPrimitrive2DIDBlock(SvgRadialGradientPrimitive2D, PRIMITIVE2D_ID_SVGRADIALGRADIENTPRIMITIVE2D)
+        // provide unique ID
+        ImplPrimitrive2DIDBlock(SvgRadialGradientPrimitive2D, PRIMITIVE2D_ID_SVGRADIALGRADIENTPRIMITIVE2D)
 
-	} // end of namespace primitive2d
+    } // end of namespace primitive2d
 } // end of namespace drawinglayer
 
 //////////////////////////////////////////////////////////////////////////////
 // SvgLinearAtomPrimitive2D class
 
 namespace drawinglayer
 {
-	namespace primitive2d
-	{
+    namespace primitive2d
+    {
         Primitive2DSequence SvgLinearAtomPrimitive2D::create2DDecomposition(const geometry::ViewInformation2D& /*rViewInformation*/) const
         {
             Primitive2DSequence xRetval;
             const double fDelta(getOffsetB() - getOffsetA());
 
             if(!basegfx::fTools::equalZero(fDelta))
             {
-                if(getColorA() == getColorB())
-                {
-                    const basegfx::B2DPolygon aPolygon(
-                        basegfx::tools::createPolygonFromRect(
-                            basegfx::B2DRange(
-                                getOffsetA() - getOverlapping(), 
-                                0.0, 
-                                getOffsetB() + getOverlapping(), 
-                                1.0)));
-
-                    xRetval.realloc(1);
-                    xRetval[0] = new PolyPolygonColorPrimitive2D(
-                        basegfx::B2DPolyPolygon(aPolygon), 
-                        getColorA());
-                }
-                else
-                {
-                    // calc discrete length to change color all 2.5 pixels
-                    sal_uInt32 nSteps(basegfx::fround(fDelta / (getDiscreteUnit() * 2.5)));
+                // use one discrete unit for overlap (one pixel)
+                const double fDiscreteUnit(getDiscreteUnit());
 
-                    // use color distance, assume to do every 3rd
-                    const double fColorDistance(getColorA().getDistance(getColorB()));
-                    const sal_uInt32 nColorSteps(basegfx::fround(fColorDistance * (255.0 * 0.3)));
-                    nSteps = std::min(nSteps, nColorSteps);
+                // use color distance and discrete lengths to calculate step count
+                const sal_uInt32 nSteps(calculateStepsForSvgGradient(getColorA(), getColorB(), fDelta, fDiscreteUnit));
 
-                    // roughly cut when too big
-                    nSteps = std::min(nSteps, sal_uInt32(100));
-                    nSteps = std::max(nSteps, sal_uInt32(1));
+                // prepare loop and polygon (with overlap for linear gradients)
+                double fStart(0.0);
+                double fStep(fDelta / nSteps);
+                const basegfx::B2DPolygon aPolygon(
+                    basegfx::tools::createPolygonFromRect(
+                        basegfx::B2DRange(
+                            getOffsetA() - fDiscreteUnit, 
+                            0.0, 
+                            getOffsetA() + fStep + fDiscreteUnit, 
+                            1.0)));
 
-                    // preapare iteration
-                    double fStart(0.0);
-                    double fStep(fDelta / nSteps);
+                // loop and create primitives
+                xRetval.realloc(nSteps);
 
-                    xRetval.realloc(nSteps);
+                for(sal_uInt32 a(0); a < nSteps; a++, fStart += fStep)
+                {
+                    basegfx::B2DPolygon aNew(aPolygon);
 
-                    for(sal_uInt32 a(0); a < nSteps; a++, fStart += fStep)
-                    {
-                        const double fLeft(getOffsetA() + fStart);
-                        const double fRight(fLeft + fStep);
-                        const basegfx::B2DPolygon aPolygon(
-                            basegfx::tools::createPolygonFromRect(
-                                basegfx::B2DRange(
-                                    fLeft - getOverlapping(), 
-                                    0.0, 
-                                    fRight + getOverlapping(), 
-                                    1.0)));
-
-                        xRetval[a] = new PolyPolygonColorPrimitive2D(
-                            basegfx::B2DPolyPolygon(aPolygon), 
-                            basegfx::interpolate(getColorA(), getColorB(), fStart/fDelta));
-                    }
+                    aNew.transform(basegfx::tools::createTranslateB2DHomMatrix(fStart, 0.0));
+                    xRetval[a] = new PolyPolygonColorPrimitive2D(
+                        basegfx::B2DPolyPolygon(aNew), 
+                        basegfx::interpolate(getColorA(), getColorB(), fStart/fDelta));
                 }
             }
 
             return xRetval;
         }
 
+        SvgLinearAtomPrimitive2D::SvgLinearAtomPrimitive2D(
+            const basegfx::BColor& aColorA, double fOffsetA,
+            const basegfx::BColor& aColorB, double fOffsetB)
+        :   DiscreteMetricDependentPrimitive2D(),
+            maColorA(aColorA),
+            maColorB(aColorB),
+            mfOffsetA(fOffsetA),
+            mfOffsetB(fOffsetB)
+        {
+            if(mfOffsetA > mfOffsetB)
+            {
+                OSL_ENSURE(false, "Wrong offset order (!)");
+                ::std::swap(mfOffsetA, mfOffsetB);
+            }
+        }
+
         bool SvgLinearAtomPrimitive2D::operator==(const BasePrimitive2D& rPrimitive) const
         {
-			if(DiscreteMetricDependentPrimitive2D::operator==(rPrimitive))
-			{
-				const SvgLinearAtomPrimitive2D& rCompare = static_cast< const SvgLinearAtomPrimitive2D& >(rPrimitive);
+            if(DiscreteMetricDependentPrimitive2D::operator==(rPrimitive))
+            {
+                const SvgLinearAtomPrimitive2D& rCompare = static_cast< const SvgLinearAtomPrimitive2D& >(rPrimitive);
 
-				return (getColorA() == rCompare.getColorA()
+                return (getColorA() == rCompare.getColorA()
                     && getColorB() == rCompare.getColorB()
                     && getOffsetA() == rCompare.getOffsetA()
-                    && getOffsetB() == rCompare.getOffsetB()
-                    && getOverlapping() == rCompare.getOverlapping());
-			}
+                    && getOffsetB() == rCompare.getOffsetB());
+            }
 
-			return false;
+            return false;
         }
 
-		// provide unique ID
-		ImplPrimitrive2DIDBlock(SvgLinearAtomPrimitive2D, PRIMITIVE2D_ID_SVGLINEARATOMPRIMITIVE2D)
+        // provide unique ID
+        ImplPrimitrive2DIDBlock(SvgLinearAtomPrimitive2D, PRIMITIVE2D_ID_SVGLINEARATOMPRIMITIVE2D)
 
     } // end of namespace primitive2d
 } // end of namespace drawinglayer
 
 //////////////////////////////////////////////////////////////////////////////
 // SvgRadialAtomPrimitive2D class
 
 namespace drawinglayer
 {
-	namespace primitive2d
-	{
+    namespace primitive2d
+    {
         Primitive2DSequence SvgRadialAtomPrimitive2D::create2DDecomposition(const geometry::ViewInformation2D& /*rViewInformation*/) const
         {
             Primitive2DSequence xRetval;
             const double fDeltaScale(getScaleB() - getScaleA());
 
             if(!basegfx::fTools::equalZero(fDeltaScale))
             {
-                if(getColorA() == getColorB())
-                {
-                    basegfx::B2DPolygon aPolygonA(basegfx::tools::createPolygonFromUnitCircle());
-                    basegfx::B2DPolygon aPolygonB(basegfx::tools::createPolygonFromUnitCircle());
-                    double fScaleA(getScaleA());
-                    const double fScaleB(getScaleB());
+                // use one discrete unit for overlap (one pixel)
+                const double fDiscreteUnit(getDiscreteUnit());
 
-                    if(fScaleA > getOverlapping())
-                    {
-                        fScaleA -= getOverlapping();
-                    }
+                // use color distance and discrete lengths to calculate step count
+                const sal_uInt32 nSteps(calculateStepsForSvgGradient(getColorA(), getColorB(), fDeltaScale, fDiscreteUnit));
 
-                    const bool bUseA(basegfx::fTools::equalZero(fScaleA));
+                // prepare loop (outside to inside, full polygons, no polypolygons with holes)
+                double fEndScale(getScaleB());
+                double fStepScale(fDeltaScale / nSteps);
 
-                    if(getTranslateSet())
-                    {
-                        if(bUseA)
-                        {
-                            aPolygonA.transform(
-                                basegfx::tools::createScaleTranslateB2DHomMatrix(
-                                    fScaleA,
-                                    fScaleA,
-                                    getTranslateA().getX(),
-                                    getTranslateA().getY()));
-                        }
+                // loop and create primitives
+                xRetval.realloc(nSteps);
+
+                for(sal_uInt32 a(0); a < nSteps; a++, fEndScale -= fStepScale)
+                {
+                    const double fUnitScale(fEndScale/fDeltaScale);
+                    basegfx::B2DHomMatrix aTransform;
 
-                        aPolygonB.transform(
-                            basegfx::tools::createScaleTranslateB2DHomMatrix(
-                                fScaleB,
-                                fScaleB,
-                                getTranslateB().getX(),
-                                getTranslateB().getY()));
+                    if(isTranslateSet())
+                    {
+                        const basegfx::B2DVector aTranslate(
+                            basegfx::interpolate(
+                                getTranslateA(), 
+                                getTranslateB(), 
+                                fUnitScale));
+
+                        aTransform = basegfx::tools::createScaleTranslateB2DHomMatrix(
+                            fEndScale,
+                            fEndScale,
+                            aTranslate.getX(),
+                            aTranslate.getY());
                     }
                     else
                     {
-                        if(bUseA)
-                        {
-                            aPolygonA.transform(
-                                basegfx::tools::createScaleB2DHomMatrix(
-                                    fScaleA,
-                                    fScaleA));
-                        }
-
-                        aPolygonB.transform(
-                            basegfx::tools::createScaleB2DHomMatrix(
-                                fScaleB,
-                                fScaleB));
+                        aTransform = basegfx::tools::createScaleB2DHomMatrix(
+                            fEndScale,
+                            fEndScale);
                     }
 
-                    basegfx::B2DPolyPolygon aPolyPolygon(aPolygonB);
-                    
-                    if(bUseA)
-                    {
-                        aPolyPolygon.append(aPolygonA);
-                    }
+                    basegfx::B2DPolygon aNew(basegfx::tools::createPolygonFromUnitCircle());
 
-                    xRetval.realloc(1);
-                    
-                    xRetval[0] = new PolyPolygonColorPrimitive2D(
-                        aPolyPolygon, 
-                        getColorA());
+                    aNew.transform(aTransform);
+                    xRetval[a] = new PolyPolygonColorPrimitive2D(
+                        basegfx::B2DPolyPolygon(aNew), 
+                        basegfx::interpolate(getColorA(), getColorB(), fUnitScale));
                 }
-                else
-                {
-                    // calc discrete length to change color all 2.5 pixels
-                    sal_uInt32 nSteps(basegfx::fround(fDeltaScale / (getDiscreteUnit() * 2.5)));
-
-                    // use color distance, assume to do every 3rd
-                    const double fColorDistance(getColorA().getDistance(getColorB()));
-                    const sal_uInt32 nColorSteps(basegfx::fround(fColorDistance * (255.0 * 0.3)));
-                    nSteps = std::min(nSteps, nColorSteps);
-
-                    // roughly cut when too big
-                    nSteps = std::min(nSteps, sal_uInt32(100));
-                    nSteps = std::max(nSteps, sal_uInt32(1));
-
-                    // preapare iteration
-                    double fStartScale(0.0);
-                    double fStepScale(fDeltaScale / nSteps);
-
-                    xRetval.realloc(nSteps);
-
-                    for(sal_uInt32 a(0); a < nSteps; a++, fStartScale += fStepScale)
-                    {
-                        double fScaleA(getScaleA() + fStartScale);
-                        const double fScaleB(fScaleA + fStepScale);
-                        const double fUnitScale(fStartScale/fDeltaScale);
-                        basegfx::B2DPolygon aPolygonA(basegfx::tools::createPolygonFromUnitCircle());
-                        basegfx::B2DPolygon aPolygonB(basegfx::tools::createPolygonFromUnitCircle());
+            }
 
-                        if(fScaleA > getOverlapping())
-                        {
-                            fScaleA -= getOverlapping();
-                        }
-                        
-                        const bool bUseA(basegfx::fTools::equalZero(fScaleA));
+            return xRetval;
+        }
 
-                        if(getTranslateSet())
-                        {
-                            const double fUnitScaleEnd((fStartScale + fStepScale)/fDeltaScale);
-                            const basegfx::B2DVector aTranslateB(basegfx::interpolate(getTranslateA(), getTranslateB(), fUnitScaleEnd));
+        SvgRadialAtomPrimitive2D::SvgRadialAtomPrimitive2D(
+            const basegfx::BColor& aColorA, double fScaleA, const basegfx::B2DVector& rTranslateA,
+            const basegfx::BColor& aColorB, double fScaleB, const basegfx::B2DVector& rTranslateB)
+        :   DiscreteMetricDependentPrimitive2D(),
+            maColorA(aColorA),
+            maColorB(aColorB),
+            mfScaleA(fScaleA),
+            mfScaleB(fScaleB),
+            mpTranslate(0)
+        {
+            // check and evtl. set translations
+            if(!rTranslateA.equal(rTranslateB))
+            {
+                mpTranslate = new VectorPair(rTranslateA, rTranslateB);
+            }
 
-                            if(bUseA)
-                            {
-                                const basegfx::B2DVector aTranslateA(basegfx::interpolate(getTranslateA(), getTranslateB(), fUnitScale));
-                                
-                                aPolygonA.transform(
-                                    basegfx::tools::createScaleTranslateB2DHomMatrix(
-                                        fScaleA,
-                                        fScaleA,
-                                        aTranslateA.getX(),
-                                        aTranslateA.getY()));
-                            }
+            // scale A and B have to be positive
+            mfScaleA = ::std::max(mfScaleA, 0.0);
+            mfScaleB = ::std::max(mfScaleB, 0.0);
 
-                            aPolygonB.transform(
-                                basegfx::tools::createScaleTranslateB2DHomMatrix(
-                                    fScaleB,
-                                    fScaleB,
-                                    aTranslateB.getX(),
-                                    aTranslateB.getY()));
-                        }
-                        else
-                        {
-                            if(bUseA)
-                            {
-                                aPolygonA.transform(
-                                    basegfx::tools::createScaleB2DHomMatrix(
-                                        fScaleA,
-                                        fScaleA));
-                            }
+            // scale B has to be bigger than scale A; swap if different
+            if(mfScaleA > mfScaleB)
+            {
+                OSL_ENSURE(false, "Wrong offset order (!)");
+                ::std::swap(mfScaleA, mfScaleB);
 
-                            aPolygonB.transform(
-                                basegfx::tools::createScaleB2DHomMatrix(
-                                    fScaleB,
-                                    fScaleB));
-                        }
+                if(mpTranslate)
+                {
+                    ::std::swap(mpTranslate->maTranslateA, mpTranslate->maTranslateB);
+                }
+            }
+        }
 
-                        basegfx::B2DPolyPolygon aPolyPolygon(aPolygonB);
-                        
-                        if(bUseA)
-                        {
-                            aPolyPolygon.append(aPolygonA);
-                        }
+        SvgRadialAtomPrimitive2D::SvgRadialAtomPrimitive2D(
+            const basegfx::BColor& aColorA, double fScaleA,
+            const basegfx::BColor& aColorB, double fScaleB)
+        :   DiscreteMetricDependentPrimitive2D(),
+            maColorA(aColorA),
+            maColorB(aColorB),
+            mfScaleA(fScaleA),
+            mfScaleB(fScaleB),
+            mpTranslate(0)
+        {
+            // scale A and B have to be positive
+            mfScaleA = ::std::max(mfScaleA, 0.0);
+            mfScaleB = ::std::max(mfScaleB, 0.0);
 
-                        xRetval[nSteps - 1 - a] = new PolyPolygonColorPrimitive2D(
-                            aPolyPolygon, 
-                            basegfx::interpolate(getColorA(), getColorB(), fUnitScale));
-                    }
-                }
+            // scale B has to be bigger than scale A; swap if different
+            if(mfScaleA > mfScaleB)
+            {
+                OSL_ENSURE(false, "Wrong offset order (!)");
+                ::std::swap(mfScaleA, mfScaleB);
             }
+        }
 
-            return xRetval;
+        SvgRadialAtomPrimitive2D::~SvgRadialAtomPrimitive2D()
+        {
+            if(mpTranslate)
+            {
+                delete mpTranslate;
+                mpTranslate = 0;
+            }
         }
 
         bool SvgRadialAtomPrimitive2D::operator==(const BasePrimitive2D& rPrimitive) const
         {
-			if(DiscreteMetricDependentPrimitive2D::operator==(rPrimitive))
-			{
-				const SvgRadialAtomPrimitive2D& rCompare = static_cast< const SvgRadialAtomPrimitive2D& >(rPrimitive);
+            if(DiscreteMetricDependentPrimitive2D::operator==(rPrimitive))
+            {
+                const SvgRadialAtomPrimitive2D& rCompare = static_cast< const SvgRadialAtomPrimitive2D& >(rPrimitive);
 
-				return (getColorA() == rCompare.getColorA()
+                if(getColorA() == rCompare.getColorA()
                     && getColorB() == rCompare.getColorB()
                     && getScaleA() == rCompare.getScaleA()
-                    && getScaleB() == rCompare.getScaleB()
-                    && getTranslateA() == rCompare.getTranslateA()
-                    && getTranslateB() == rCompare.getTranslateB()
-                    && getOverlapping() == rCompare.getOverlapping());
-			}
+                    && getScaleB() == rCompare.getScaleB())
+                {
+                    if(isTranslateSet() && rCompare.isTranslateSet())
+                    {
+                        return (getTranslateA() == rCompare.getTranslateA()
+                            && getTranslateB() == rCompare.getTranslateB());
+                    }
+                    else if(!isTranslateSet() && !rCompare.isTranslateSet())
+                    {
+                        return true;
+                    }
+                }
+            }
 
-			return false;
+            return false;
         }
 
-		// provide unique ID
-		ImplPrimitrive2DIDBlock(SvgRadialAtomPrimitive2D, PRIMITIVE2D_ID_SVGRADIALATOMPRIMITIVE2D)
+        // provide unique ID
+        ImplPrimitrive2DIDBlock(SvgRadialAtomPrimitive2D, PRIMITIVE2D_ID_SVGRADIALATOMPRIMITIVE2D)
 
-	} // end of namespace primitive2d
+    } // end of namespace primitive2d
 } // end of namespace drawinglayer
 
 //////////////////////////////////////////////////////////////////////////////
 // eof
Index: main/drawinglayer/source/primitive2d/textdecoratedprimitive2d.cxx
===================================================================
--- main/drawinglayer/source/primitive2d/textdecoratedprimitive2d.cxx	(revision 1232674)
+++ main/drawinglayer/source/primitive2d/textdecoratedprimitive2d.cxx	(revision 1239200)
@@ -23,23 +23,20 @@
 
 // MARKER(update_precomp.py): autogen include statement, do not remove
 #include "precompiled_drawinglayer.hxx"
 
 #include <drawinglayer/primitive2d/textdecoratedprimitive2d.hxx>
-#include <drawinglayer/primitive2d/textlayoutdevice.hxx>
 #include <drawinglayer/primitive2d/polygonprimitive2d.hxx>
 #include <drawinglayer/attribute/strokeattribute.hxx>
 #include <drawinglayer/primitive2d/drawinglayer_primitivetypes2d.hxx>
 #include <basegfx/matrix/b2dhommatrixtools.hxx>
-#include <comphelper/processfactory.hxx>
-#include <com/sun/star/i18n/WordType.hpp>
 #include <drawinglayer/primitive2d/texteffectprimitive2d.hxx>
 #include <drawinglayer/primitive2d/shadowprimitive2d.hxx>
-#include <com/sun/star/i18n/XBreakIterator.hpp>
 #include <drawinglayer/primitive2d/transformprimitive2d.hxx>
 #include <drawinglayer/primitive2d/textlineprimitive2d.hxx>
 #include <drawinglayer/primitive2d/textstrikeoutprimitive2d.hxx>
+#include <drawinglayer/primitive2d/textbreakuphelper.hxx>
 
 //////////////////////////////////////////////////////////////////////////////
 
 namespace drawinglayer
 {
@@ -163,235 +160,50 @@
 			}
 
             // TODO: Handle Font Emphasis Above/Below
         }
 
-		void TextDecoratedPortionPrimitive2D::impCorrectTextBoundary(::com::sun::star::i18n::Boundary& rNextWordBoundary) const
-		{
-			// truncate aNextWordBoundary to min/max possible values. This is necessary since the word start may be
-			// before/after getTextPosition() when a long string is the content and getTextPosition()
-			// is right inside a word. Same for end.
-			const sal_Int32 aMinPos(static_cast< sal_Int32 >(getTextPosition()));
-			const sal_Int32 aMaxPos(aMinPos + static_cast< sal_Int32 >(getTextLength()));
-
-			if(rNextWordBoundary.startPos < aMinPos)
-			{
-				rNextWordBoundary.startPos = aMinPos;
-			}
-			else if(rNextWordBoundary.startPos > aMaxPos)
-			{
-				rNextWordBoundary.startPos = aMaxPos;
-			}
-
-			if(rNextWordBoundary.endPos < aMinPos)
-			{
-				rNextWordBoundary.endPos = aMinPos;
-			}
-			else if(rNextWordBoundary.endPos > aMaxPos)
-			{
-				rNextWordBoundary.endPos = aMaxPos;
-			}
-		}
-
-        void TextDecoratedPortionPrimitive2D::impSplitSingleWords(
-            std::vector< Primitive2DReference >& rTarget,
-            basegfx::tools::B2DHomMatrixBufferedOnDemandDecompose& rDecTrans) const
+		Primitive2DSequence TextDecoratedPortionPrimitive2D::create2DDecomposition(const geometry::ViewInformation2D& /*rViewInformation*/) const
         {
-            // break iterator support
-            // made static so it only needs to be fetched once, even with many single
-            // constructed VclMetafileProcessor2D. It's still incarnated on demand,
-            // but exists for OOo runtime now by purpose.
-            static ::com::sun::star::uno::Reference< ::com::sun::star::i18n::XBreakIterator > xLocalBreakIterator;
-
-			if(!xLocalBreakIterator.is())
-            {
-                ::com::sun::star::uno::Reference< ::com::sun::star::lang::XMultiServiceFactory > xMSF(::comphelper::getProcessServiceFactory());
-                xLocalBreakIterator.set(xMSF->createInstance(rtl::OUString::createFromAscii("com.sun.star.i18n.BreakIterator")), ::com::sun::star::uno::UNO_QUERY);
-            }
-
-            if(xLocalBreakIterator.is() && getTextLength())
+            if(getWordLineMode())
             {
-                // init word iterator, get first word and truncate to possibilities
-                ::com::sun::star::i18n::Boundary aNextWordBoundary(xLocalBreakIterator->getWordBoundary(
-                    getText(), getTextPosition(), getLocale(), ::com::sun::star::i18n::WordType::ANYWORD_IGNOREWHITESPACES, sal_True));
+                // support for single word mode; split to single word primitives
+                // using TextBreakupHelper
+                const TextBreakupHelper aTextBreakupHelper(*this);
+                const Primitive2DSequence aBroken(aTextBreakupHelper.getResult(BreakupUnit_word));
 
-                if(aNextWordBoundary.endPos == getTextPosition())
+                if(aBroken.hasElements())
                 {
-                    // backward hit, force next word
-                    aNextWordBoundary = xLocalBreakIterator->getWordBoundary(
-                        getText(), getTextPosition() + 1, getLocale(), ::com::sun::star::i18n::WordType::ANYWORD_IGNOREWHITESPACES, sal_True);
+                    // was indeed split to several words, use as result
+                    return aBroken;
+                }
+                else
+                {
+                    // no split, was already a single word. Continue to
+                    // decompse local entity
                 }
-
-				impCorrectTextBoundary(aNextWordBoundary);
-
-				// prepare new font attributes WITHOUT outline
-                const attribute::FontAttribute aNewFontAttribute(
-                    getFontAttribute().getFamilyName(),
-                    getFontAttribute().getStyleName(),
-                    getFontAttribute().getWeight(),
-                    getFontAttribute().getSymbol(),
-                    getFontAttribute().getVertical(),
-                    getFontAttribute().getItalic(),
-                    false,             // no outline anymore, handled locally
-                    getFontAttribute().getRTL(),
-                    getFontAttribute().getBiDiStrong());
-
-				if(aNextWordBoundary.startPos == getTextPosition() && aNextWordBoundary.endPos == getTextLength())
-				{
-					// it IS only a single word, handle as one word
-	                impCreateGeometryContent(rTarget, rDecTrans, getText(), getTextPosition(), getTextLength(), getDXArray(), aNewFontAttribute);
-				}
-				else
-				{
-					// prepare TextLayouter
-					const bool bNoDXArray(getDXArray().empty());
-					TextLayouterDevice aTextLayouter;
-
-					if(bNoDXArray)
-					{
-						// ..but only completely when no DXArray
-						aTextLayouter.setFontAttribute(
-                            getFontAttribute(), 
-                            rDecTrans.getScale().getX(), 
-                            rDecTrans.getScale().getY(),
-                            getLocale());
-					}
-
-					// do iterate over single words
-					while(aNextWordBoundary.startPos != aNextWordBoundary.endPos)
-					{
-						// prepare values for new portion
-						const xub_StrLen nNewTextStart(static_cast< xub_StrLen >(aNextWordBoundary.startPos));
-						const xub_StrLen nNewTextEnd(static_cast< xub_StrLen >(aNextWordBoundary.endPos));
-
-						// prepare transform for the single word
-						basegfx::B2DHomMatrix aNewTransform;
-						::std::vector< double > aNewDXArray;
-						const bool bNewStartIsNotOldStart(nNewTextStart > getTextPosition());
-
-						if(!bNoDXArray)
-						{
-							// prepare new DXArray for the single word
-							aNewDXArray = ::std::vector< double >(
-								getDXArray().begin() + static_cast< sal_uInt32 >(nNewTextStart - getTextPosition()), 
-								getDXArray().begin() + static_cast< sal_uInt32 >(nNewTextEnd - getTextPosition()));
-						}
-
-						if(bNewStartIsNotOldStart)
-						{
-							// needs to be moved to a new start position
-							double fOffset(0.0);
-							
-							if(bNoDXArray)
-							{
-								// evaluate using TextLayouter
-								fOffset = aTextLayouter.getTextWidth(getText(), getTextPosition(), nNewTextStart);
-							}
-							else
-							{
-								// get from DXArray
-								const sal_uInt32 nIndex(static_cast< sal_uInt32 >(nNewTextStart - getTextPosition()));
-								fOffset = getDXArray()[nIndex - 1];
-							}
-
-                            // need offset without FontScale for building the new transformation. The
-                            // new transformation will be multiplied with the current text transformation
-                            // so FontScale would be double
-							double fOffsetNoScale(fOffset);
-                            const double fFontScaleX(rDecTrans.getScale().getX());
-                            
-                            if(!basegfx::fTools::equal(fFontScaleX, 1.0) 
-                                && !basegfx::fTools::equalZero(fFontScaleX))
-                            {
-                                fOffsetNoScale /= fFontScaleX;
-                            }
-
-							// apply needed offset to transformation
-                            aNewTransform.translate(fOffsetNoScale, 0.0);
-
-							if(!bNoDXArray)
-							{
-								// DXArray values need to be corrected with the offset, too. Here,
-                                // take the scaled offset since the DXArray is scaled
-								const sal_uInt32 nArraySize(aNewDXArray.size());
-
-								for(sal_uInt32 a(0); a < nArraySize; a++)
-								{
-									aNewDXArray[a] -= fOffset;
-								}
-							}
-						}
-
-						// add text transformation to new transformation
-						aNewTransform *= rDecTrans.getB2DHomMatrix();
-
-						// create geometry content for the single word. Do not forget
-						// to use the new transformation
-						basegfx::tools::B2DHomMatrixBufferedOnDemandDecompose aDecTrans(aNewTransform);
-						
-						impCreateGeometryContent(rTarget, aDecTrans, getText(), nNewTextStart, 
-							nNewTextEnd - nNewTextStart, aNewDXArray, aNewFontAttribute);
-
-                        if(aNextWordBoundary.endPos >= getTextPosition() + getTextLength())
-                        {
-                            // end reached
-                            aNextWordBoundary.startPos = aNextWordBoundary.endPos;
-                        }
-                        else
-                        {
-                            // get new word portion
-                            const sal_Int32 nLastEndPos(aNextWordBoundary.endPos);
-
-                            aNextWordBoundary = xLocalBreakIterator->getWordBoundary(
-                                getText(), aNextWordBoundary.endPos, getLocale(), 
-                                ::com::sun::star::i18n::WordType::ANYWORD_IGNOREWHITESPACES, sal_True);
-
-                            if(nLastEndPos == aNextWordBoundary.endPos)
-                            {
-                                // backward hit, force next word
-                                aNextWordBoundary = xLocalBreakIterator->getWordBoundary(
-                                    getText(), nLastEndPos + 1, getLocale(), 
-                                    ::com::sun::star::i18n::WordType::ANYWORD_IGNOREWHITESPACES, sal_True);
-                            }
-
-                            impCorrectTextBoundary(aNextWordBoundary);
-                        }
-					}
-				}
             }
-        }
-
-		Primitive2DSequence TextDecoratedPortionPrimitive2D::create2DDecomposition(const geometry::ViewInformation2D& /*rViewInformation*/) const
-        {
             std::vector< Primitive2DReference > aNewPrimitives;
             basegfx::tools::B2DHomMatrixBufferedOnDemandDecompose aDecTrans(getTextTransform());
             Primitive2DSequence aRetval;
 
             // create basic geometry such as SimpleTextPrimitive, Overline, Underline,
             // Strikeout, etc...
-            if(getWordLineMode())
-            {
-                // support for single word mode
-                impSplitSingleWords(aNewPrimitives, aDecTrans);
-            }
-            else
-            {
-                // prepare new font attributes WITHOUT outline
-                const attribute::FontAttribute aNewFontAttribute(
-                    getFontAttribute().getFamilyName(),
-                    getFontAttribute().getStyleName(),
-                    getFontAttribute().getWeight(),
-                    getFontAttribute().getSymbol(),
-                    getFontAttribute().getVertical(),
-                    getFontAttribute().getItalic(),
-                    false,             // no outline anymore, handled locally
-                    getFontAttribute().getRTL(),
-                    getFontAttribute().getBiDiStrong());
+            // prepare new font attributes WITHOUT outline
+            const attribute::FontAttribute aNewFontAttribute(
+                getFontAttribute().getFamilyName(),
+                getFontAttribute().getStyleName(),
+                getFontAttribute().getWeight(),
+                getFontAttribute().getSymbol(),
+                getFontAttribute().getVertical(),
+                getFontAttribute().getItalic(),
+                false,             // no outline anymore, handled locally
+                getFontAttribute().getRTL(),
+                getFontAttribute().getBiDiStrong());
 
-				// handle as one word
-                impCreateGeometryContent(aNewPrimitives, aDecTrans, getText(), getTextPosition(), getTextLength(), getDXArray(), aNewFontAttribute);
-            }
+			// handle as one word
+            impCreateGeometryContent(aNewPrimitives, aDecTrans, getText(), getTextPosition(), getTextLength(), getDXArray(), aNewFontAttribute);
 
             // convert to Primitive2DSequence
             const sal_uInt32 nMemberCount(aNewPrimitives.size());
 
 			if(nMemberCount)
@@ -540,10 +352,20 @@
 			mbEmphasisMarkBelow(bEmphasisMarkBelow),
 			mbShadow(bShadow)
 		{
 		}
 
+        bool TextDecoratedPortionPrimitive2D::decoratedIsNeeded() const
+        {
+            return (TEXT_LINE_NONE != getFontOverline()
+                 || TEXT_LINE_NONE != getFontUnderline()
+                 || TEXT_STRIKEOUT_NONE != getTextStrikeout()
+                 || TEXT_EMPHASISMARK_NONE != getTextEmphasisMark()
+                 || TEXT_RELIEF_NONE != getTextRelief()
+                 || getShadow());
+        }
+
 		bool TextDecoratedPortionPrimitive2D::operator==(const BasePrimitive2D& rPrimitive) const
 		{
 			if(TextSimplePortionPrimitive2D::operator==(rPrimitive))
 			{
 				const TextDecoratedPortionPrimitive2D& rCompare = (TextDecoratedPortionPrimitive2D&)rPrimitive;
@@ -568,19 +390,11 @@
         // #i96475#
         // Added missing implementation. Decorations may (will) stick out of the text's
         // inking area, so add them if needed
 		basegfx::B2DRange TextDecoratedPortionPrimitive2D::getB2DRange(const geometry::ViewInformation2D& rViewInformation) const
 		{
-			const bool bDecoratedIsNeeded(
-                TEXT_LINE_NONE != getFontOverline()
-             || TEXT_LINE_NONE != getFontUnderline()
-             || TEXT_STRIKEOUT_NONE != getTextStrikeout()
-             || TEXT_EMPHASISMARK_NONE != getTextEmphasisMark()
-             || TEXT_RELIEF_NONE != getTextRelief()
-             || getShadow());
-
-            if(bDecoratedIsNeeded)
+            if(decoratedIsNeeded())
             {
                 // decoration is used, fallback to BufferedDecompositionPrimitive2D::getB2DRange which uses
                 // the own local decomposition for computation and thus creates all necessary
                 // geometric objects
                 return BufferedDecompositionPrimitive2D::getB2DRange(rViewInformation);
Index: main/drawinglayer/source/primitive2d/hiddengeometryprimitive2d.cxx
===================================================================
--- main/drawinglayer/source/primitive2d/hiddengeometryprimitive2d.cxx	(revision 1232674)
+++ main/drawinglayer/source/primitive2d/hiddengeometryprimitive2d.cxx	(revision 1239200)
@@ -1,39 +1,25 @@
-/*************************************************************************
- *
- *  OpenOffice.org - a multi-platform office productivity suite
- *
- *  $RCSfile: hittestprimitive3d.cxx,v $
- *
- *  $Revision: 1.1.2.1 $
- *
- *  last change: $Author: aw $ $Date: 2008/09/25 17:12:14 $
- *
- *  The Contents of this file are made available subject to
- *  the terms of GNU Lesser General Public License Version 2.1.
- *
- *
- *    GNU Lesser General Public License Version 2.1
- *    =============================================
- *    Copyright 2005 by Sun Microsystems, Inc.
- *    901 San Antonio Road, Palo Alto, CA 94303, USA
- *
- *    This library is free software; you can redistribute it and/or
- *    modify it under the terms of the GNU Lesser General Public
- *    License version 2.1, as published by the Free Software Foundation.
- *
- *    This library is distributed in the hope that it will be useful,
- *    but WITHOUT ANY WARRANTY; without even the implied warranty of
- *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
- *    Lesser General Public License for more details.
- *
- *    You should have received a copy of the GNU Lesser General Public
- *    License along with this library; if not, write to the Free Software
- *    Foundation, Inc., 59 Temple Place, Suite 330, Boston,
- *    MA  02111-1307  USA
- *
- ************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
 
 // MARKER(update_precomp.py): autogen include statement, do not remove
 #include "precompiled_drawinglayer.hxx"
 
 #include <drawinglayer/primitive2d/hiddengeometryprimitive2d.hxx>
Index: main/drawinglayer/source/primitive2d/textbreakuphelper.cxx
===================================================================
--- main/drawinglayer/source/primitive2d/textbreakuphelper.cxx	(revision 1232674)
+++ main/drawinglayer/source/primitive2d/textbreakuphelper.cxx	(revision 1239200)
@@ -26,160 +26,161 @@
 #include <drawinglayer/primitive2d/textdecoratedprimitive2d.hxx>
 #include <com/sun/star/i18n/XBreakIterator.hpp>
 #include <comphelper/processfactory.hxx>
 #include <com/sun/star/i18n/CharacterIteratorMode.hdl>
 #include <com/sun/star/i18n/WordType.hpp>
+#include <com/sun/star/i18n/CharType.hpp>
 
 //////////////////////////////////////////////////////////////////////////////
 
 namespace drawinglayer
 {
-	namespace primitive2d
-	{
-        TextBreakupHelper::TextBreakupHelper(const Primitive2DReference& rxSource)
-        :   mxSource(rxSource),
+    namespace primitive2d
+    {
+        TextBreakupHelper::TextBreakupHelper(const TextSimplePortionPrimitive2D& rSource)
+        :   mrSource(rSource),
             mxResult(),
-            mpSource(dynamic_cast< const TextSimplePortionPrimitive2D* >(rxSource.get())),
             maTextLayouter(),
             maDecTrans(),
             mbNoDXArray(false)
         {
-            if(mpSource)
-            {
-                maDecTrans = mpSource->getTextTransform();
-                mbNoDXArray = mpSource->getDXArray().empty();
+            OSL_ENSURE(dynamic_cast< const TextSimplePortionPrimitive2D* >(&mrSource), "TextBreakupHelper with illegal primitive created (!)");
+            maDecTrans = mrSource.getTextTransform();
+            mbNoDXArray = mrSource.getDXArray().empty();
 
-                if(mbNoDXArray)
-                {
-                    // init TextLayouter when no dxarray
-				    maTextLayouter.setFontAttribute(
-                        mpSource->getFontAttribute(), 
-                        maDecTrans.getScale().getX(), 
-                        maDecTrans.getScale().getY(),
-                        mpSource->getLocale());
-                }
+            if(mbNoDXArray)
+            {
+                // init TextLayouter when no dxarray
+                maTextLayouter.setFontAttribute(
+                    mrSource.getFontAttribute(), 
+                    maDecTrans.getScale().getX(), 
+                    maDecTrans.getScale().getY(),
+                    mrSource.getLocale());
             }
         }
 
         TextBreakupHelper::~TextBreakupHelper()
         {
         }
 
-        void TextBreakupHelper::breakupPortion(Primitive2DVector& rTempResult, sal_uInt32 nIndex, sal_uInt32 nLength)
+        void TextBreakupHelper::breakupPortion(Primitive2DVector& rTempResult, sal_uInt32 nIndex, sal_uInt32 nLength, bool bWordLineMode)
         {
-            if(mpSource && nLength && !(nIndex == mpSource->getTextPosition() && nLength == mpSource->getTextLength()))
+            if(nLength && !(nIndex == mrSource.getTextPosition() && nLength == mrSource.getTextLength()))
             {
- 				// prepare values for new portion
-				basegfx::B2DHomMatrix aNewTransform;
-				::std::vector< double > aNewDXArray;
-				const bool bNewStartIsNotOldStart(nIndex > mpSource->getTextPosition());
-
-				if(!mbNoDXArray)
-				{
-					// prepare new DXArray for the single word
-					aNewDXArray = ::std::vector< double >(
-						mpSource->getDXArray().begin() + (nIndex - mpSource->getTextPosition()), 
-						mpSource->getDXArray().begin() + ((nIndex + nLength) - mpSource->getTextPosition()));
-				}
-
-				if(bNewStartIsNotOldStart)
-				{
-					// needs to be moved to a new start position
-					double fOffset(0.0);
-							
-					if(mbNoDXArray)
-					{
-						// evaluate using TextLayouter
-						fOffset = maTextLayouter.getTextWidth(mpSource->getText(), mpSource->getTextPosition(), nIndex);
-					}
-					else
-					{
-						// get from DXArray
-						const sal_uInt32 nIndex2(static_cast< sal_uInt32 >(nIndex - mpSource->getTextPosition()));
-						fOffset = mpSource->getDXArray()[nIndex2 - 1];
-					}
+                // prepare values for new portion
+                basegfx::B2DHomMatrix aNewTransform;
+                ::std::vector< double > aNewDXArray;
+                const bool bNewStartIsNotOldStart(nIndex > mrSource.getTextPosition());
+
+                if(!mbNoDXArray)
+                {
+                    // prepare new DXArray for the single word
+                    aNewDXArray = ::std::vector< double >(
+                        mrSource.getDXArray().begin() + (nIndex - mrSource.getTextPosition()), 
+                        mrSource.getDXArray().begin() + ((nIndex + nLength) - mrSource.getTextPosition()));
+                }
+
+                if(bNewStartIsNotOldStart)
+                {
+                    // needs to be moved to a new start position
+                    double fOffset(0.0);
+
+                    if(mbNoDXArray)
+                    {
+                        // evaluate using TextLayouter
+                        fOffset = maTextLayouter.getTextWidth(mrSource.getText(), mrSource.getTextPosition(), nIndex);
+                    }
+                    else
+                    {
+                        // get from DXArray
+                        const sal_uInt32 nIndex2(static_cast< sal_uInt32 >(nIndex - mrSource.getTextPosition()));
+                        fOffset = mrSource.getDXArray()[nIndex2 - 1];
+                    }
 
                     // need offset without FontScale for building the new transformation. The
                     // new transformation will be multiplied with the current text transformation
                     // so FontScale would be double
-					double fOffsetNoScale(fOffset);
+                    double fOffsetNoScale(fOffset);
                     const double fFontScaleX(maDecTrans.getScale().getX());
-                            
+
                     if(!basegfx::fTools::equal(fFontScaleX, 1.0) 
                         && !basegfx::fTools::equalZero(fFontScaleX))
                     {
                         fOffsetNoScale /= fFontScaleX;
                     }
 
-					// apply needed offset to transformation
+                    // apply needed offset to transformation
                     aNewTransform.translate(fOffsetNoScale, 0.0);
 
-					if(!mbNoDXArray)
-					{
-						// DXArray values need to be corrected with the offset, too. Here,
+                    if(!mbNoDXArray)
+                    {
+                        // DXArray values need to be corrected with the offset, too. Here,
                         // take the scaled offset since the DXArray is scaled
-						const sal_uInt32 nArraySize(aNewDXArray.size());
+                        const sal_uInt32 nArraySize(aNewDXArray.size());
 
-						for(sal_uInt32 a(0); a < nArraySize; a++)
-						{
-							aNewDXArray[a] -= fOffset;
-						}
-					}
-				}
+                        for(sal_uInt32 a(0); a < nArraySize; a++)
+                        {
+                            aNewDXArray[a] -= fOffset;
+                        }
+                    }
+                }
 
-				// add text transformation to new transformation
-				aNewTransform = maDecTrans.getB2DHomMatrix() * aNewTransform;
+                // add text transformation to new transformation
+                aNewTransform = maDecTrans.getB2DHomMatrix() * aNewTransform;
 
                 // callback to allow evtl. changes
                 const bool bCreate(allowChange(rTempResult.size(), aNewTransform, nIndex, nLength));
 
                 if(bCreate)
                 {
                     // check if we have a decorated primitive as source
                     const TextDecoratedPortionPrimitive2D* pTextDecoratedPortionPrimitive2D = 
-                        dynamic_cast< const TextDecoratedPortionPrimitive2D* >(mpSource);
+                        dynamic_cast< const TextDecoratedPortionPrimitive2D* >(&mrSource);
 
                     if(pTextDecoratedPortionPrimitive2D)
                     {
                         // create a TextDecoratedPortionPrimitive2D
-	                    rTempResult.push_back(
+                        rTempResult.push_back(
                             new TextDecoratedPortionPrimitive2D(
                                 aNewTransform,
-                                mpSource->getText(),
-				                nIndex,
-				                nLength,
+                                mrSource.getText(),
+                                nIndex,
+                                nLength,
                                 aNewDXArray,
-                                mpSource->getFontAttribute(),
-                                mpSource->getLocale(),
-                                mpSource->getFontColor(),
+                                mrSource.getFontAttribute(),
+                                mrSource.getLocale(),
+                                mrSource.getFontColor(),
                             
                                 pTextDecoratedPortionPrimitive2D->getOverlineColor(),
                                 pTextDecoratedPortionPrimitive2D->getTextlineColor(),
                                 pTextDecoratedPortionPrimitive2D->getFontOverline(),
                                 pTextDecoratedPortionPrimitive2D->getFontUnderline(),
                                 pTextDecoratedPortionPrimitive2D->getUnderlineAbove(),
                                 pTextDecoratedPortionPrimitive2D->getTextStrikeout(),
-                                pTextDecoratedPortionPrimitive2D->getWordLineMode(),
+
+                                // reset WordLineMode when BreakupUnit_word is executed; else copy original
+                                bWordLineMode ? false : pTextDecoratedPortionPrimitive2D->getWordLineMode(),
+
                                 pTextDecoratedPortionPrimitive2D->getTextEmphasisMark(),
                                 pTextDecoratedPortionPrimitive2D->getEmphasisMarkAbove(),
                                 pTextDecoratedPortionPrimitive2D->getEmphasisMarkBelow(),
                                 pTextDecoratedPortionPrimitive2D->getTextRelief(),
                                 pTextDecoratedPortionPrimitive2D->getShadow()));
                     }
                     else
                     {
                         // create a SimpleTextPrimitive
-	                    rTempResult.push_back(
+                        rTempResult.push_back(
                             new TextSimplePortionPrimitive2D(
                                 aNewTransform,
-                                mpSource->getText(),
-				                nIndex,
-				                nLength,
+                                mrSource.getText(),
+                                nIndex,
+                                nLength,
                                 aNewDXArray,
-                                mpSource->getFontAttribute(),
-                                mpSource->getLocale(),
-                                mpSource->getFontColor()));
+                                mrSource.getFontAttribute(),
+                                mrSource.getLocale(),
+                                mrSource.getFontColor()));
                     }
                 }
             }
         }
 
@@ -188,11 +189,11 @@
             return true;
         }
 
         void TextBreakupHelper::breakup(BreakupUnit aBreakupUnit)
         {
-            if(mpSource && mpSource->getTextLength())
+            if(mrSource.getTextLength())
             {
                 Primitive2DVector aTempResult;
                 static ::com::sun::star::uno::Reference< ::com::sun::star::i18n::XBreakIterator > xBreakIterator;
 
                 if(!xBreakIterator.is())
@@ -201,14 +202,14 @@
                     xBreakIterator.set(xMSF->createInstance(rtl::OUString::createFromAscii("com.sun.star.i18n.BreakIterator")), ::com::sun::star::uno::UNO_QUERY);
                 }
 
                 if(xBreakIterator.is())
                 {
-                    const rtl::OUString& rTxt = mpSource->getText();
-                    const sal_Int32 nTextLength(mpSource->getTextLength());
-                    const ::com::sun::star::lang::Locale& rLocale = mpSource->getLocale();
-                    const sal_Int32 nTextPosition(mpSource->getTextPosition());
+                    const rtl::OUString& rTxt = mrSource.getText();
+                    const sal_Int32 nTextLength(mrSource.getTextLength());
+                    const ::com::sun::star::lang::Locale& rLocale = mrSource.getLocale();
+                    const sal_Int32 nTextPosition(mrSource.getTextPosition());
                     sal_Int32 nCurrent(nTextPosition);
                     
                     switch(aBreakupUnit)
                     {
                         case BreakupUnit_character:
@@ -219,17 +220,17 @@
 
                             for(; a < nTextPosition + nTextLength; a++)
                             {
                                 if(a == nNextCellBreak)
                                 {
-                                    breakupPortion(aTempResult, nCurrent, a - nCurrent);
+                                    breakupPortion(aTempResult, nCurrent, a - nCurrent, false);
                                     nCurrent = a;
                                     nNextCellBreak = xBreakIterator->nextCharacters(rTxt, a, rLocale, ::com::sun::star::i18n::CharacterIteratorMode::SKIPCELL, 1, nDone);
                                 }
                             }
 
-                            breakupPortion(aTempResult, nCurrent, a - nCurrent);
+                            breakupPortion(aTempResult, nCurrent, a - nCurrent, false);
                             break;
                         }
                         case BreakupUnit_word:
                         {
                             ::com::sun::star::i18n::Boundary nNextWordBoundary(xBreakIterator->getWordBoundary(rTxt, nTextPosition, rLocale, ::com::sun::star::i18n::WordType::ANY_WORD, sal_True));
@@ -237,17 +238,35 @@
                             
                             for(; a < nTextPosition + nTextLength; a++)
                             {
                                 if(a == nNextWordBoundary.endPos)
                                 {
-                                    breakupPortion(aTempResult, nCurrent, a - nCurrent);
+                                    if(a > nCurrent)
+                                    {
+                                        breakupPortion(aTempResult, nCurrent, a - nCurrent, true);
+                                    }
+
                                     nCurrent = a;
+
+                                    // skip spaces (maybe enhanced with a bool later if needed)
+                                    {
+                                        const sal_Int32 nEndOfSpaces(xBreakIterator->endOfCharBlock(rTxt, a, rLocale, ::com::sun::star::i18n::CharType::SPACE_SEPARATOR));
+
+                                        if(nEndOfSpaces > a)
+                                        {
+                                            nCurrent = nEndOfSpaces;
+                                        }
+                                    }
+
                                     nNextWordBoundary = xBreakIterator->getWordBoundary(rTxt, a + 1, rLocale, ::com::sun::star::i18n::WordType::ANY_WORD, sal_True);
                                 }
                             }
 
-                            breakupPortion(aTempResult, nCurrent, a - nCurrent);
+                            if(a > nCurrent)
+                            {
+                                breakupPortion(aTempResult, nCurrent, a - nCurrent, true);
+                            }
                             break;
                         }
                         case BreakupUnit_sentence:
                         {
                             sal_Int32 nNextSentenceBreak(xBreakIterator->endOfSentence(rTxt, nTextPosition, rLocale));
@@ -255,17 +274,17 @@
                             
                             for(; a < nTextPosition + nTextLength; a++)
                             {
                                 if(a == nNextSentenceBreak)
                                 {
-                                    breakupPortion(aTempResult, nCurrent, a - nCurrent);
+                                    breakupPortion(aTempResult, nCurrent, a - nCurrent, false);
                                     nCurrent = a;
                                     nNextSentenceBreak = xBreakIterator->endOfSentence(rTxt, a + 1, rLocale);
                                 }
                             }
 
-                            breakupPortion(aTempResult, nCurrent, a - nCurrent);
+                            breakupPortion(aTempResult, nCurrent, a - nCurrent, false);
                             break;
                         }
                     }
                 }
                 
@@ -273,22 +292,18 @@
             }
         }
 
         const Primitive2DSequence& TextBreakupHelper::getResult(BreakupUnit aBreakupUnit) const
         {
-            if(mxResult.hasElements())
-            {
-                return mxResult;
-            }
-            else if(mpSource)
+            if(!mxResult.hasElements())
             {
                 const_cast< TextBreakupHelper* >(this)->breakup(aBreakupUnit);
             }
 
             return mxResult;
         }
 
-	} // end of namespace primitive2d
+    } // end of namespace primitive2d
 } // end of namespace drawinglayer
 
 //////////////////////////////////////////////////////////////////////////////
 // eof
Index: main/drawinglayer/source/primitive2d/transparenceprimitive2d.cxx
===================================================================
--- main/drawinglayer/source/primitive2d/transparenceprimitive2d.cxx	(revision 1232674)
+++ main/drawinglayer/source/primitive2d/transparenceprimitive2d.cxx	(revision 1239200)
@@ -1,39 +1,25 @@
-/*************************************************************************
- *
- *  OpenOffice.org - a multi-platform office productivity suite
- *
- *  $RCSfile: alphaprimitive2d.cxx,v $
- *
- *  $Revision: 1.5 $
- *
- *  last change: $Author: aw $ $Date: 2008-05-27 14:11:20 $
- *
- *  The Contents of this file are made available subject to
- *  the terms of GNU Lesser General Public License Version 2.1.
- *
- *
- *    GNU Lesser General Public License Version 2.1
- *    =============================================
- *    Copyright 2005 by Sun Microsystems, Inc.
- *    901 San Antonio Road, Palo Alto, CA 94303, USA
- *
- *    This library is free software; you can redistribute it and/or
- *    modify it under the terms of the GNU Lesser General Public
- *    License version 2.1, as published by the Free Software Foundation.
- *
- *    This library is distributed in the hope that it will be useful,
- *    but WITHOUT ANY WARRANTY; without even the implied warranty of
- *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
- *    Lesser General Public License for more details.
- *
- *    You should have received a copy of the GNU Lesser General Public
- *    License along with this library; if not, write to the Free Software
- *    Foundation, Inc., 59 Temple Place, Suite 330, Boston,
- *    MA  02111-1307  USA
- *
- ************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
 
 // MARKER(update_precomp.py): autogen include statement, do not remove
 #include "precompiled_drawinglayer.hxx"
 
 #include <drawinglayer/primitive2d/transparenceprimitive2d.hxx>
Index: main/drawinglayer/source/primitive2d/unifiedtransparenceprimitive2d.cxx
===================================================================
--- main/drawinglayer/source/primitive2d/unifiedtransparenceprimitive2d.cxx	(revision 1232674)
+++ main/drawinglayer/source/primitive2d/unifiedtransparenceprimitive2d.cxx	(revision 1239200)
@@ -1,39 +1,25 @@
-/*************************************************************************
- *
- *  OpenOffice.org - a multi-platform office productivity suite
- *
- *  $RCSfile: UnifiedTransparencePrimitive2D.cxx,v $
- *
- *  $Revision: 1.5 $
- *
- *  last change: $Author: aw $ $Date: 2008-05-27 14:11:20 $
- *
- *  The Contents of this file are made available subject to
- *  the terms of GNU Lesser General Public License Version 2.1.
- *
- *
- *    GNU Lesser General Public License Version 2.1
- *    =============================================
- *    Copyright 2005 by Sun Microsystems, Inc.
- *    901 San Antonio Road, Palo Alto, CA 94303, USA
- *
- *    This library is free software; you can redistribute it and/or
- *    modify it under the terms of the GNU Lesser General Public
- *    License version 2.1, as published by the Free Software Foundation.
- *
- *    This library is distributed in the hope that it will be useful,
- *    but WITHOUT ANY WARRANTY; without even the implied warranty of
- *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
- *    Lesser General Public License for more details.
- *
- *    You should have received a copy of the GNU Lesser General Public
- *    License along with this library; if not, write to the Free Software
- *    Foundation, Inc., 59 Temple Place, Suite 330, Boston,
- *    MA  02111-1307  USA
- *
- ************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
 
 // MARKER(update_precomp.py): autogen include statement, do not remove
 #include "precompiled_drawinglayer.hxx"
 
 #include <drawinglayer/primitive2d/unifiedtransparenceprimitive2d.hxx>
Index: main/i18npool/source/search/textsearch.cxx
===================================================================
--- main/i18npool/source/search/textsearch.cxx	(revision 1232674)
+++ main/i18npool/source/search/textsearch.cxx	(revision 1239200)
@@ -61,11 +61,11 @@
     TransliterationModules_ignoreHyuByu_ja_JP |
     TransliterationModules_ignoreSeZe_ja_JP |
     TransliterationModules_ignoreIandEfollowedByYa_ja_JP |
     TransliterationModules_ignoreKiKuFollowedBySa_ja_JP |
     TransliterationModules_ignoreProlongedSoundMark_ja_JP;
-static const sal_Int32 SIMPLE_TRANS_MASK = ~(COMPLEX_TRANS_MASK_TMP | TransliterationModules_IGNORE_WIDTH) | TransliterationModules_FULLWIDTH_HALFWIDTH;
+static const sal_Int32 SIMPLE_TRANS_MASK = TransliterationModules_HIRAGANA_KATAKANA | TransliterationModules_FULLWIDTH_HALFWIDTH;
 static const sal_Int32 COMPLEX_TRANS_MASK = COMPLEX_TRANS_MASK_TMP | TransliterationModules_IGNORE_KANA | TransliterationModules_FULLWIDTH_HALFWIDTH;
     // Above 2 transliteration is simple but need to take effect in
     // complex transliteration
 
 TextSearch::TextSearch(const Reference < XMultiServiceFactory > & rxMSF)
@@ -174,31 +174,12 @@
     switch( aSrchPara.algorithmType)
     {
 		case SearchAlgorithms_REGEXP:
 			fnForward = &TextSearch::RESrchFrwrd;
 			fnBackward = &TextSearch::RESrchBkwrd;
-
-			{
-			sal_uInt32 nIcuSearchFlags = 0;
-			// map com::sun::star::util::SearchFlags to ICU uregex.h flags
-			// TODO: REG_EXTENDED, REG_NOT_BEGINOFLINE, REG_NOT_ENDOFLINE
-			// REG_NEWLINE is neither defined properly nor used anywhere => not implemented
-			// REG_NOSUB is not used anywhere => not implemented
-			// NORM_WORD_ONLY is only used for SearchAlgorithm==Absolute
-			// LEV_RELAXED is only used for SearchAlgorithm==Approximate
-			// why is even ALL_IGNORE_CASE deprecated in UNO? because of transliteration taking care of it???
-			if( (aSrchPara.searchFlag & com::sun::star::util::SearchFlags::ALL_IGNORE_CASE) != 0)
-				nIcuSearchFlags |= UREGEX_CASE_INSENSITIVE;
-			UErrorCode nIcuErr = U_ZERO_ERROR;
-			// assumption: transliteration doesn't mangle regexp control chars
-			OUString& rPatternStr = (aSrchPara.transliterateFlags & SIMPLE_TRANS_MASK) ? sSrchStr
-					: ((aSrchPara.transliterateFlags & COMPLEX_TRANS_MASK) ? sSrchStr2 : aSrchPara.searchString);
-			const IcuUniString aIcuSearchPatStr( rPatternStr.getStr(), rPatternStr.getLength());
-			pRegexMatcher = new RegexMatcher( aIcuSearchPatStr, nIcuSearchFlags, nIcuErr);
-			if( nIcuErr)
-				{ delete pRegexMatcher; pRegexMatcher = NULL;}
-			} break;
+			RESrchPrepare( aSrchPara);
+			break;
 
 		case SearchAlgorithms_APPROXIMATE:
             fnForward = &TextSearch::ApproxSrchFrwrd;
             fnBackward = &TextSearch::ApproxSrchBkwrd;
 
@@ -718,10 +699,45 @@
         nCmpIdx -= nSuchIdx;
     }
     return aRet;
 }
 
+void TextSearch::RESrchPrepare( const ::com::sun::star::util::SearchOptions& rOptions)
+{
+	// select the transliterated pattern string
+	const OUString& rPatternStr = 
+		(rOptions.transliterateFlags & SIMPLE_TRANS_MASK) ? sSrchStr
+		: ((rOptions.transliterateFlags & COMPLEX_TRANS_MASK) ? sSrchStr2 : rOptions.searchString);
+
+	sal_uInt32 nIcuSearchFlags = UREGEX_UWORD; // request UAX#29 unicode capability
+	// map com::sun::star::util::SearchFlags to ICU uregex.h flags
+	// TODO: REG_EXTENDED, REG_NOT_BEGINOFLINE, REG_NOT_ENDOFLINE
+	// REG_NEWLINE is neither properly defined nor used anywhere => not implemented
+	// REG_NOSUB is not used anywhere => not implemented
+	// NORM_WORD_ONLY is only used for SearchAlgorithm==Absolute
+	// LEV_RELAXED is only used for SearchAlgorithm==Approximate
+	// why is even ALL_IGNORE_CASE deprecated in UNO? because of transliteration taking care of it???
+	if( (rOptions.searchFlag & com::sun::star::util::SearchFlags::ALL_IGNORE_CASE) != 0)
+		nIcuSearchFlags |= UREGEX_CASE_INSENSITIVE;
+	UErrorCode nIcuErr = U_ZERO_ERROR;
+	// assumption: transliteration didn't mangle regexp control chars
+	IcuUniString aIcuSearchPatStr( rPatternStr.getStr(), rPatternStr.getLength());
+#if 1
+	// for conveniance specific syntax elements of the old regex engine are emulated
+	// by using regular word boundary matching \b to replace \< and \>
+	static const IcuUniString aChevronPattern( "\\<|\\>", -1, IcuUniString::kInvariant);
+	static const IcuUniString aChevronReplace( "\\b", -1, IcuUniString::kInvariant);
+	static RegexMatcher aChevronMatcher( aChevronPattern, 0, nIcuErr);
+	aChevronMatcher.reset( aIcuSearchPatStr);
+	aIcuSearchPatStr = aChevronMatcher.replaceAll( aChevronReplace, nIcuErr);
+	aChevronMatcher.reset();
+#endif
+	pRegexMatcher = new RegexMatcher( aIcuSearchPatStr, nIcuSearchFlags, nIcuErr);
+	if( nIcuErr)
+		{ delete pRegexMatcher; pRegexMatcher = NULL;}
+}
+
 //---------------------------------------------------------------------------
 
 SearchResult TextSearch::RESrchFrwrd( const OUString& searchStr,
                                       sal_Int32 startPos, sal_Int32 endPos )
             throw(RuntimeException)
Index: main/sccomp/source/solver/solver.cxx
===================================================================
--- main/sccomp/source/solver/solver.cxx	(revision 1232674)
+++ main/sccomp/source/solver/solver.cxx	(revision 1239200)
@@ -19,16 +19,11 @@
  * 
  *************************************************************/
 
 
 
-#undef LANGUAGE_NONE
-#define WINAPI __stdcall
-#define LoadInverseLib FALSE
-#define LoadLanguageLib FALSE
-#include <lpsolve/lp_lib.h>
-#undef LANGUAGE_NONE
+#include <coinmp/CoinMP.h>
 
 #include "solver.hxx"
 #include "solver.hrc"
 
 #include <com/sun/star/beans/XPropertySet.hpp>
@@ -308,16 +303,10 @@
         throw uno::RuntimeException();
 
     maStatus = OUString();
     mbSuccess = false;
 
-    if ( mnEpsilonLevel < EPS_TIGHT || mnEpsilonLevel > EPS_BAGGY )
-    {
-        maStatus = lcl_GetResourceString( RID_ERROR_EPSILONLEVEL );
-        return;
-    }
-
     xModel->lockControllers();
 
     // collect variables in vector (?)
 
     std::vector<table::CellAddress> aVariableCells;
@@ -392,33 +381,36 @@
 
     if ( maStatus.getLength() )
         return;
 
     //
-    // build lp_solve model
+    // build parameter arrays for CoinMP
     //
 
-    lprec* lp = make_lp( 0, nVariables );
-    if ( !lp )
-        return;
-
-    set_outputfile( lp, const_cast<char*>( "" ) );  // no output
-
     // set objective function
 
     const std::vector<double>& rObjCoeff = aCellsHash[maObjective];
-    REAL* pObjVal = new REAL[nVariables+1];
-    pObjVal[0] = 0.0;                           // ignored
+    double* pObjectCoeffs = new double[nVariables];
     for (nVar=0; nVar<nVariables; nVar++)
-        pObjVal[nVar+1] = rObjCoeff[nVar+1];
-    set_obj_fn( lp, pObjVal );
-    delete[] pObjVal;
-    set_rh( lp, 0, rObjCoeff[0] );              // constant term of objective
+        pObjectCoeffs[nVar] = rObjCoeff[nVar+1];
+    double nObjectConst = rObjCoeff[0];             // constant term of objective
 
     // add rows
 
-    set_add_rowmode(lp, TRUE);
+    size_t nRows = maConstraints.getLength();
+    size_t nCompSize = nVariables * nRows;
+    double* pCompMatrix = new double[nCompSize];    // first collect all coefficients, row-wise
+    for (size_t i=0; i<nCompSize; i++)
+        pCompMatrix[i] = 0.0;
+
+    double* pRHS = new double[nRows];
+    char* pRowType = new char[nRows];
+    for (size_t i=0; i<nRows; i++)
+    {
+        pRHS[i] = 0.0;
+        pRowType[i] = 'N';
+    }
 
     for (sal_Int32 nConstrPos = 0; nConstrPos < maConstraints.getLength(); ++nConstrPos)
     {
         // integer constraints are set later
         sheet::SolverConstraintOperator eOp = maConstraints[nConstrPos].Operator;
@@ -436,58 +428,84 @@
                 rRightAny >>= fDirectValue;         // constant value
 
             table::CellAddress aLeftAddr = maConstraints[nConstrPos].Left;
 
             const std::vector<double>& rLeftCoeff = aCellsHash[aLeftAddr];
-            REAL* pValues = new REAL[nVariables+1];
-            pValues[0] = 0.0;                               // ignored?
+            double* pValues = &pCompMatrix[nConstrPos * nVariables];
             for (nVar=0; nVar<nVariables; nVar++)
-                pValues[nVar+1] = rLeftCoeff[nVar+1];
+                pValues[nVar] = rLeftCoeff[nVar+1];
 
             // if left hand cell has a constant term, put into rhs value
             double fRightValue = -rLeftCoeff[0];
 
             if ( bRightCell )
             {
                 const std::vector<double>& rRightCoeff = aCellsHash[aRightAddr];
                 // modify pValues with rhs coefficients
                 for (nVar=0; nVar<nVariables; nVar++)
-                    pValues[nVar+1] -= rRightCoeff[nVar+1];
+                    pValues[nVar] -= rRightCoeff[nVar+1];
 
                 fRightValue += rRightCoeff[0];      // constant term
             }
             else
                 fRightValue += fDirectValue;
 
-            int nConstrType = LE;
             switch ( eOp )
             {
-                case sheet::SolverConstraintOperator_LESS_EQUAL:    nConstrType = LE; break;
-                case sheet::SolverConstraintOperator_GREATER_EQUAL: nConstrType = GE; break;
-                case sheet::SolverConstraintOperator_EQUAL:         nConstrType = EQ; break;
+                case sheet::SolverConstraintOperator_LESS_EQUAL:    pRowType[nConstrPos] = 'L'; break;
+                case sheet::SolverConstraintOperator_GREATER_EQUAL: pRowType[nConstrPos] = 'G'; break;
+                case sheet::SolverConstraintOperator_EQUAL:         pRowType[nConstrPos] = 'E'; break;
                 default:
                     OSL_ENSURE( false, "unexpected enum type" );
             }
-            add_constraint( lp, pValues, nConstrType, fRightValue );
-
-            delete[] pValues;
+            pRHS[nConstrPos] = fRightValue;
         }
     }
 
-    set_add_rowmode(lp, FALSE);
+    // Find non-zero coefficients, column-wise
+
+    int* pMatrixBegin = new int[nVariables+1];
+    int* pMatrixCount = new int[nVariables];
+    double* pMatrix = new double[nCompSize];    // not always completely used
+    int* pMatrixIndex = new int[nCompSize];
+    int nMatrixPos = 0;
+    for (nVar=0; nVar<nVariables; nVar++)
+    {
+        int nBegin = nMatrixPos;
+        for (size_t nRow=0; nRow<nRows; nRow++)
+        {
+            double fCoeff = pCompMatrix[ nRow * nVariables + nVar ];    // row-wise
+            if ( fCoeff != 0.0 )
+            {
+                pMatrix[nMatrixPos] = fCoeff;
+                pMatrixIndex[nMatrixPos] = nRow;
+                ++nMatrixPos;
+            }
+        }
+        pMatrixBegin[nVar] = nBegin;
+        pMatrixCount[nVar] = nMatrixPos - nBegin;
+    }
+    pMatrixBegin[nVariables] = nMatrixPos;
+    delete[] pCompMatrix;
+    pCompMatrix = NULL;
 
     // apply settings to all variables
 
+    double* pLowerBounds = new double[nVariables];
+    double* pUpperBounds = new double[nVariables];
     for (nVar=0; nVar<nVariables; nVar++)
     {
-        if ( !mbNonNegative )
-            set_unbounded(lp, nVar+1);          // allow negative (default is non-negative)
-                                                //! collect bounds from constraints?
-        if ( mbInteger )
-            set_int(lp, nVar+1, TRUE);
+        pLowerBounds[nVar] = mbNonNegative ? 0.0 : -DBL_MAX;
+        pUpperBounds[nVar] = DBL_MAX;
+
+        // bounds could possibly be further restricted from single-cell constraints
     }
 
+    char* pColType = new char[nVariables];
+    for (nVar=0; nVar<nVariables; nVar++)
+        pColType[nVar] = mbInteger ? 'I' : 'C';
+
     // apply single-var integer constraints
 
     for (sal_Int32 nConstrPos = 0; nConstrPos < maConstraints.getLength(); ++nConstrPos)
     {
         sheet::SolverConstraintOperator eOp = maConstraints[nConstrPos].Operator;
@@ -498,55 +516,73 @@
             // find variable index for cell
             for (nVar=0; nVar<nVariables; nVar++)
                 if ( AddressEqual( aVariableCells[nVar], aLeftAddr ) )
                 {
                     if ( eOp == sheet::SolverConstraintOperator_INTEGER )
-                        set_int(lp, nVar+1, TRUE);
+                        pColType[nVar] = 'I';
                     else
-                        set_binary(lp, nVar+1, TRUE);
+                    {
+                        pColType[nVar] = 'B';
+                        pLowerBounds[nVar] = 0.0;
+                        pUpperBounds[nVar] = 1.0;
+                    }
                 }
         }
     }
 
-    if ( mbMaximize )
-        set_maxim(lp);
-    else
-        set_minim(lp);
+    int nObjectSense = mbMaximize ? SOLV_OBJSENS_MAX : SOLV_OBJSENS_MIN;
+
+    HPROB hProb = CoinCreateProblem("");
+    int nResult = CoinLoadProblem( hProb, nVariables, nRows, nMatrixPos, 0,
+                    nObjectSense, nObjectConst, pObjectCoeffs,
+                    pLowerBounds, pUpperBounds, pRowType, pRHS, NULL,
+                    pMatrixBegin, pMatrixCount, pMatrixIndex, pMatrix,
+                    NULL, NULL, NULL );
+    nResult = CoinLoadInteger( hProb, pColType );
+
+    delete[] pColType;
+    delete[] pMatrixIndex;
+    delete[] pMatrix;
+    delete[] pMatrixCount;
+    delete[] pMatrixBegin;
+    delete[] pUpperBounds;
+    delete[] pLowerBounds;
+    delete[] pRowType;
+    delete[] pRHS;
+    delete[] pObjectCoeffs;
 
-    if ( !mbLimitBBDepth )
-        set_bb_depthlimit( lp, 0 );
+    CoinSetRealOption( hProb, COIN_REAL_MAXSECONDS, mnTimeout );
+    CoinSetRealOption( hProb, COIN_REAL_MIPMAXSEC, mnTimeout );
 
-    set_epslevel( lp, mnEpsilonLevel );
-    set_timeout( lp, mnTimeout );
+    // TODO: handle (or remove) settings: epsilon, B&B depth
 
     // solve model
 
-    int nResult = ::solve( lp );
+    nResult = CoinCheckProblem( hProb );
+    nResult = CoinOptimizeProblem( hProb, 0 );
 
-    mbSuccess = ( nResult == OPTIMAL );
+    mbSuccess = ( nResult == SOLV_CALL_SUCCESS );
     if ( mbSuccess )
     {
         // get solution
 
         maSolution.realloc( nVariables );
+        CoinGetSolutionValues( hProb, maSolution.getArray(), NULL, NULL, NULL );
+        mfResultValue = CoinGetObjectValue( hProb );
+    }
+    else
+    {
+        int nSolutionStatus = CoinGetSolutionStatus( hProb );
+        if ( nSolutionStatus == 1 )
+            maStatus = lcl_GetResourceString( RID_ERROR_INFEASIBLE );
+        else if ( nSolutionStatus == 2 )
+            maStatus = lcl_GetResourceString( RID_ERROR_UNBOUNDED );
+        // TODO: detect timeout condition and report as RID_ERROR_TIMEOUT
+        // (currently reported as infeasible)
+    }
 
-        REAL* pResultVar = NULL;
-        get_ptr_variables( lp, &pResultVar );
-        for (nVar=0; nVar<nVariables; nVar++)
-            maSolution[nVar] = pResultVar[nVar];
-
-        mfResultValue = get_objective( lp );
-    }
-    else if ( nResult == INFEASIBLE )
-        maStatus = lcl_GetResourceString( RID_ERROR_INFEASIBLE );
-    else if ( nResult == UNBOUNDED )
-        maStatus = lcl_GetResourceString( RID_ERROR_UNBOUNDED );
-    else if ( nResult == TIMEOUT || nResult == SUBOPTIMAL )
-        maStatus = lcl_GetResourceString( RID_ERROR_TIMEOUT );
-    // SUBOPTIMAL is assumed to be caused by a timeout, and reported as an error
-
-    delete_lp( lp );
+    CoinUnloadProblem( hProb );
 }
 
 // -------------------------------------------------------------------------
 
 // XServiceInfo
Index: main/odk/examples/cpp/counter/counter.cxx
===================================================================
--- main/odk/examples/cpp/counter/counter.cxx	(revision 1232674)
+++ main/odk/examples/cpp/counter/counter.cxx	(revision 1239200)
@@ -1,38 +1,27 @@
-/*************************************************************************
- *
- *  The Contents of this file are made available subject to the terms of
- *  the BSD license.
- *  
- *  Copyright 2000, 2010 Oracle and/or its affiliates.
- *  All rights reserved.
- *
- *  Redistribution and use in source and binary forms, with or without
- *  modification, are permitted provided that the following conditions
- *  are met:
- *  1. Redistributions of source code must retain the above copyright
- *     notice, this list of conditions and the following disclaimer.
- *  2. Redistributions in binary form must reproduce the above copyright
- *     notice, this list of conditions and the following disclaimer in the
- *     documentation and/or other materials provided with the distribution.
- *  3. Neither the name of Sun Microsystems, Inc. nor the names of its
- *     contributors may be used to endorse or promote products derived
- *     from this software without specific prior written permission.
- *
- *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
- *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
- *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
- *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
- *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
- *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
- *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
- *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
- *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
- *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
- *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
- *     
- *************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
+
+
 
 /*************************************************************************
  *************************************************************************
  *
  * service implementation:	 foo.Counter
Index: main/odk/examples/cpp/counter/countermain.cxx
===================================================================
--- main/odk/examples/cpp/counter/countermain.cxx	(revision 1232674)
+++ main/odk/examples/cpp/counter/countermain.cxx	(revision 1239200)
@@ -1,38 +1,27 @@
-/*************************************************************************
- *
- *  The Contents of this file are made available subject to the terms of
- *  the BSD license.
- *  
- *  Copyright 2000, 2010 Oracle and/or its affiliates.
- *  All rights reserved.
- *
- *  Redistribution and use in source and binary forms, with or without
- *  modification, are permitted provided that the following conditions
- *  are met:
- *  1. Redistributions of source code must retain the above copyright
- *     notice, this list of conditions and the following disclaimer.
- *  2. Redistributions in binary form must reproduce the above copyright
- *     notice, this list of conditions and the following disclaimer in the
- *     documentation and/or other materials provided with the distribution.
- *  3. Neither the name of Sun Microsystems, Inc. nor the names of its
- *     contributors may be used to endorse or promote products derived
- *     from this software without specific prior written permission.
- *
- *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
- *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
- *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
- *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
- *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
- *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
- *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
- *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
- *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
- *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
- *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
- *     
- *************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
+
+
 
 /*************************************************************************
  *************************************************************************
  *
  * simple client application registering and using the counter component.
Index: main/odk/examples/cpp/DocumentLoader/DocumentLoader.cxx
===================================================================
--- main/odk/examples/cpp/DocumentLoader/DocumentLoader.cxx	(revision 1232674)
+++ main/odk/examples/cpp/DocumentLoader/DocumentLoader.cxx	(revision 1239200)
@@ -1,38 +1,27 @@
-/*************************************************************************
- *
- *  The Contents of this file are made available subject to the terms of
- *  the BSD license.
- *  
- *  Copyright 2000, 2010 Oracle and/or its affiliates.
- *  All rights reserved.
- *
- *  Redistribution and use in source and binary forms, with or without
- *  modification, are permitted provided that the following conditions
- *  are met:
- *  1. Redistributions of source code must retain the above copyright
- *     notice, this list of conditions and the following disclaimer.
- *  2. Redistributions in binary form must reproduce the above copyright
- *     notice, this list of conditions and the following disclaimer in the
- *     documentation and/or other materials provided with the distribution.
- *  3. Neither the name of Sun Microsystems, Inc. nor the names of its
- *     contributors may be used to endorse or promote products derived
- *     from this software without specific prior written permission.
- *
- *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
- *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
- *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
- *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
- *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
- *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
- *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
- *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
- *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
- *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
- *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
- *     
- *************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
+
+
 
 /*****************************************************************************
  *****************************************************************************
  *
  * Simple client application using the UnoUrlResolver service.
Index: main/odk/examples/cpp/remoteclient/remoteclient.cxx
===================================================================
--- main/odk/examples/cpp/remoteclient/remoteclient.cxx	(revision 1232674)
+++ main/odk/examples/cpp/remoteclient/remoteclient.cxx	(revision 1239200)
@@ -1,38 +1,27 @@
-/*************************************************************************
- *
- *  The Contents of this file are made available subject to the terms of
- *  the BSD license.
- *  
- *  Copyright 2000, 2010 Oracle and/or its affiliates.
- *  All rights reserved.
- *
- *  Redistribution and use in source and binary forms, with or without
- *  modification, are permitted provided that the following conditions
- *  are met:
- *  1. Redistributions of source code must retain the above copyright
- *     notice, this list of conditions and the following disclaimer.
- *  2. Redistributions in binary form must reproduce the above copyright
- *     notice, this list of conditions and the following disclaimer in the
- *     documentation and/or other materials provided with the distribution.
- *  3. Neither the name of Sun Microsystems, Inc. nor the names of its
- *     contributors may be used to endorse or promote products derived
- *     from this software without specific prior written permission.
- *
- *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
- *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
- *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
- *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
- *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
- *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
- *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
- *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
- *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
- *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
- *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
- *     
- *************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
+
+
 
 #include <stdio.h>
 #include <osl/mutex.hxx>
 #include <cppuhelper/factory.hxx>
 
Index: main/odk/examples/DevelopersGuide/Components/CppComponent/service1_impl.cxx
===================================================================
--- main/odk/examples/DevelopersGuide/Components/CppComponent/service1_impl.cxx	(revision 1232674)
+++ main/odk/examples/DevelopersGuide/Components/CppComponent/service1_impl.cxx	(revision 1239200)
@@ -1,38 +1,27 @@
-/*************************************************************************
- *
- *  The Contents of this file are made available subject to the terms of
- *  the BSD license.
- *  
- *  Copyright 2000, 2010 Oracle and/or its affiliates.
- *  All rights reserved.
- *
- *  Redistribution and use in source and binary forms, with or without
- *  modification, are permitted provided that the following conditions
- *  are met:
- *  1. Redistributions of source code must retain the above copyright
- *     notice, this list of conditions and the following disclaimer.
- *  2. Redistributions in binary form must reproduce the above copyright
- *     notice, this list of conditions and the following disclaimer in the
- *     documentation and/or other materials provided with the distribution.
- *  3. Neither the name of Sun Microsystems, Inc. nor the names of its
- *     contributors may be used to endorse or promote products derived
- *     from this software without specific prior written permission.
- *
- *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
- *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
- *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
- *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
- *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
- *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
- *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
- *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
- *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
- *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
- *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
- *     
- *************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
+
+
 
 #include <osl/interlck.h>
 #include <osl/mutex.hxx>
 #include <rtl/uuid.h>
 #include <cppuhelper/factory.hxx>
Index: main/odk/examples/DevelopersGuide/Components/CppComponent/service2_impl.cxx
===================================================================
--- main/odk/examples/DevelopersGuide/Components/CppComponent/service2_impl.cxx	(revision 1232674)
+++ main/odk/examples/DevelopersGuide/Components/CppComponent/service2_impl.cxx	(revision 1239200)
@@ -1,38 +1,27 @@
-/*************************************************************************
- *
- *  The Contents of this file are made available subject to the terms of
- *  the BSD license.
- *  
- *  Copyright 2000, 2010 Oracle and/or its affiliates.
- *  All rights reserved.
- *
- *  Redistribution and use in source and binary forms, with or without
- *  modification, are permitted provided that the following conditions
- *  are met:
- *  1. Redistributions of source code must retain the above copyright
- *     notice, this list of conditions and the following disclaimer.
- *  2. Redistributions in binary form must reproduce the above copyright
- *     notice, this list of conditions and the following disclaimer in the
- *     documentation and/or other materials provided with the distribution.
- *  3. Neither the name of Sun Microsystems, Inc. nor the names of its
- *     contributors may be used to endorse or promote products derived
- *     from this software without specific prior written permission.
- *
- *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
- *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
- *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
- *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
- *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
- *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
- *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
- *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
- *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
- *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
- *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
- *     
- *************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
+
+
 
 #include <cppuhelper/implbase3.hxx> // "3" implementing three interfaces
 #include <cppuhelper/factory.hxx>
 #include <cppuhelper/implementationentry.hxx>
 
Index: main/odk/examples/DevelopersGuide/Components/CppComponent/TestCppComponent.cxx
===================================================================
--- main/odk/examples/DevelopersGuide/Components/CppComponent/TestCppComponent.cxx	(revision 1232674)
+++ main/odk/examples/DevelopersGuide/Components/CppComponent/TestCppComponent.cxx	(revision 1239200)
@@ -1,38 +1,27 @@
-/*************************************************************************
- *
- *  The Contents of this file are made available subject to the terms of
- *  the BSD license.
- *  
- *  Copyright 2000, 2010 Oracle and/or its affiliates.
- *  All rights reserved.
- *
- *  Redistribution and use in source and binary forms, with or without
- *  modification, are permitted provided that the following conditions
- *  are met:
- *  1. Redistributions of source code must retain the above copyright
- *     notice, this list of conditions and the following disclaimer.
- *  2. Redistributions in binary form must reproduce the above copyright
- *     notice, this list of conditions and the following disclaimer in the
- *     documentation and/or other materials provided with the distribution.
- *  3. Neither the name of Sun Microsystems, Inc. nor the names of its
- *     contributors may be used to endorse or promote products derived
- *     from this software without specific prior written permission.
- *
- *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
- *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
- *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
- *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
- *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
- *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
- *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
- *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
- *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
- *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
- *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
- *     
- *************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
+
+
 
 #include <stdio.h>
 #include <sal/main.h>
 #include <cppuhelper/bootstrap.hxx>
 #include <com/sun/star/bridge/XUnoUrlResolver.hpp>
Index: main/odk/examples/DevelopersGuide/Components/Addons/ProtocolHandlerAddon_cpp/component.cxx
===================================================================
--- main/odk/examples/DevelopersGuide/Components/Addons/ProtocolHandlerAddon_cpp/component.cxx	(revision 1232674)
+++ main/odk/examples/DevelopersGuide/Components/Addons/ProtocolHandlerAddon_cpp/component.cxx	(revision 1239200)
@@ -1,38 +1,27 @@
-/*************************************************************************
- *
- *  The Contents of this file are made available subject to the terms of
- *  the BSD license.
- *  
- *  Copyright 2000, 2010 Oracle and/or its affiliates.
- *  All rights reserved.
- *
- *  Redistribution and use in source and binary forms, with or without
- *  modification, are permitted provided that the following conditions
- *  are met:
- *  1. Redistributions of source code must retain the above copyright
- *     notice, this list of conditions and the following disclaimer.
- *  2. Redistributions in binary form must reproduce the above copyright
- *     notice, this list of conditions and the following disclaimer in the
- *     documentation and/or other materials provided with the distribution.
- *  3. Neither the name of Sun Microsystems, Inc. nor the names of its
- *     contributors may be used to endorse or promote products derived
- *     from this software without specific prior written permission.
- *
- *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
- *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
- *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
- *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
- *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
- *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
- *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
- *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
- *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
- *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
- *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
- *     
- *************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
+
+
 
 #include <stdio.h>
 #include <rtl/ustring.hxx>
 #include <cppuhelper/queryinterface.hxx> // helper for queryInterface() impl
 #include <cppuhelper/factory.hxx> // helper for component factory
Index: main/odk/examples/DevelopersGuide/Components/Addons/ProtocolHandlerAddon_cpp/addon.cxx
===================================================================
--- main/odk/examples/DevelopersGuide/Components/Addons/ProtocolHandlerAddon_cpp/addon.cxx	(revision 1232674)
+++ main/odk/examples/DevelopersGuide/Components/Addons/ProtocolHandlerAddon_cpp/addon.cxx	(revision 1239200)
@@ -1,38 +1,27 @@
-/*************************************************************************
- *
- *  The Contents of this file are made available subject to the terms of
- *  the BSD license.
- *  
- *  Copyright 2000, 2010 Oracle and/or its affiliates.
- *  All rights reserved.
- *
- *  Redistribution and use in source and binary forms, with or without
- *  modification, are permitted provided that the following conditions
- *  are met:
- *  1. Redistributions of source code must retain the above copyright
- *     notice, this list of conditions and the following disclaimer.
- *  2. Redistributions in binary form must reproduce the above copyright
- *     notice, this list of conditions and the following disclaimer in the
- *     documentation and/or other materials provided with the distribution.
- *  3. Neither the name of Sun Microsystems, Inc. nor the names of its
- *     contributors may be used to endorse or promote products derived
- *     from this software without specific prior written permission.
- *
- *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
- *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
- *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
- *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
- *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
- *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
- *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
- *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
- *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
- *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
- *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
- *     
- *************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
+
+
 
 #include <addon.hxx>
 #include <osl/diagnose.h>
 #include <rtl/ustring.hxx>
 #include <com/sun/star/lang/XMultiServiceFactory.hpp>
Index: main/odk/examples/DevelopersGuide/ProfUNO/SimpleBootstrap_cpp/SimpleBootstrap_cpp.cxx
===================================================================
--- main/odk/examples/DevelopersGuide/ProfUNO/SimpleBootstrap_cpp/SimpleBootstrap_cpp.cxx	(revision 1232674)
+++ main/odk/examples/DevelopersGuide/ProfUNO/SimpleBootstrap_cpp/SimpleBootstrap_cpp.cxx	(revision 1239200)
@@ -1,38 +1,27 @@
-/*************************************************************************
- *
- *  The Contents of this file are made available subject to the terms of
- *  the BSD license.
- *  
- *  Copyright 2000, 2010 Oracle and/or its affiliates.
- *  All rights reserved.
- *
- *  Redistribution and use in source and binary forms, with or without
- *  modification, are permitted provided that the following conditions
- *  are met:
- *  1. Redistributions of source code must retain the above copyright
- *     notice, this list of conditions and the following disclaimer.
- *  2. Redistributions in binary form must reproduce the above copyright
- *     notice, this list of conditions and the following disclaimer in the
- *     documentation and/or other materials provided with the distribution.
- *  3. Neither the name of Sun Microsystems, Inc. nor the names of its
- *     contributors may be used to endorse or promote products derived
- *     from this software without specific prior written permission.
- *
- *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
- *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
- *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
- *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
- *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
- *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
- *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
- *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
- *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
- *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
- *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
- *     
- *************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
+
+
 
 #include <stdio.h>
 
 #include <sal/main.h>
 #include <cppuhelper/bootstrap.hxx>
Index: main/odk/examples/DevelopersGuide/ProfUNO/CppBinding/office_connect.cxx
===================================================================
--- main/odk/examples/DevelopersGuide/ProfUNO/CppBinding/office_connect.cxx	(revision 1232674)
+++ main/odk/examples/DevelopersGuide/ProfUNO/CppBinding/office_connect.cxx	(revision 1239200)
@@ -1,38 +1,27 @@
-/*************************************************************************
- *
- *  The Contents of this file are made available subject to the terms of
- *  the BSD license.
- *  
- *  Copyright 2000, 2010 Oracle and/or its affiliates.
- *  All rights reserved.
- *
- *  Redistribution and use in source and binary forms, with or without
- *  modification, are permitted provided that the following conditions
- *  are met:
- *  1. Redistributions of source code must retain the above copyright
- *     notice, this list of conditions and the following disclaimer.
- *  2. Redistributions in binary form must reproduce the above copyright
- *     notice, this list of conditions and the following disclaimer in the
- *     documentation and/or other materials provided with the distribution.
- *  3. Neither the name of Sun Microsystems, Inc. nor the names of its
- *     contributors may be used to endorse or promote products derived
- *     from this software without specific prior written permission.
- *
- *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
- *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
- *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
- *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
- *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
- *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
- *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
- *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
- *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
- *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
- *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
- *     
- *************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
+
+
 
 #include <stdio.h>
 
 #include <sal/main.h>
 
Index: main/odk/examples/DevelopersGuide/ProfUNO/CppBinding/string_samples.cxx
===================================================================
--- main/odk/examples/DevelopersGuide/ProfUNO/CppBinding/string_samples.cxx	(revision 1232674)
+++ main/odk/examples/DevelopersGuide/ProfUNO/CppBinding/string_samples.cxx	(revision 1239200)
@@ -1,38 +1,27 @@
-/*************************************************************************
- *
- *  The Contents of this file are made available subject to the terms of
- *  the BSD license.
- *  
- *  Copyright 2000, 2010 Oracle and/or its affiliates.
- *  All rights reserved.
- *
- *  Redistribution and use in source and binary forms, with or without
- *  modification, are permitted provided that the following conditions
- *  are met:
- *  1. Redistributions of source code must retain the above copyright
- *     notice, this list of conditions and the following disclaimer.
- *  2. Redistributions in binary form must reproduce the above copyright
- *     notice, this list of conditions and the following disclaimer in the
- *     documentation and/or other materials provided with the distribution.
- *  3. Neither the name of Sun Microsystems, Inc. nor the names of its
- *     contributors may be used to endorse or promote products derived
- *     from this software without specific prior written permission.
- *
- *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
- *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
- *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
- *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
- *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
- *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
- *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
- *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
- *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
- *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
- *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
- *     
- *************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
+
+
 
 #include <stdio.h>
 
 #include <sal/main.h>
 
Index: main/odk/examples/DevelopersGuide/ProfUNO/Lifetime/object_lifetime.cxx
===================================================================
--- main/odk/examples/DevelopersGuide/ProfUNO/Lifetime/object_lifetime.cxx	(revision 1232674)
+++ main/odk/examples/DevelopersGuide/ProfUNO/Lifetime/object_lifetime.cxx	(revision 1239200)
@@ -1,38 +1,27 @@
-/*************************************************************************
- *
- *  The Contents of this file are made available subject to the terms of
- *  the BSD license.
- *  
- *  Copyright 2000, 2010 Oracle and/or its affiliates.
- *  All rights reserved.
- *
- *  Redistribution and use in source and binary forms, with or without
- *  modification, are permitted provided that the following conditions
- *  are met:
- *  1. Redistributions of source code must retain the above copyright
- *     notice, this list of conditions and the following disclaimer.
- *  2. Redistributions in binary form must reproduce the above copyright
- *     notice, this list of conditions and the following disclaimer in the
- *     documentation and/or other materials provided with the distribution.
- *  3. Neither the name of Sun Microsystems, Inc. nor the names of its
- *     contributors may be used to endorse or promote products derived
- *     from this software without specific prior written permission.
- *
- *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
- *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
- *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
- *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
- *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
- *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
- *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
- *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
- *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
- *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
- *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
- *     
- *************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
+
+
 
 #include <stdio.h>
 
 #include <cppuhelper/weak.hxx>
 
Index: main/odk/examples/DevelopersGuide/OfficeDev/FilterDevelopment/FlatXmlFilter_cpp/FlatXml.cxx
===================================================================
--- main/odk/examples/DevelopersGuide/OfficeDev/FilterDevelopment/FlatXmlFilter_cpp/FlatXml.cxx	(revision 1232674)
+++ main/odk/examples/DevelopersGuide/OfficeDev/FilterDevelopment/FlatXmlFilter_cpp/FlatXml.cxx	(revision 1239200)
@@ -1,38 +1,27 @@
-/*************************************************************************
- *
- *  The Contents of this file are made available subject to the terms of
- *  the BSD license.
- *  
- *  Copyright 2000, 2010 Oracle and/or its affiliates.
- *  All rights reserved.
- *
- *  Redistribution and use in source and binary forms, with or without
- *  modification, are permitted provided that the following conditions
- *  are met:
- *  1. Redistributions of source code must retain the above copyright
- *     notice, this list of conditions and the following disclaimer.
- *  2. Redistributions in binary form must reproduce the above copyright
- *     notice, this list of conditions and the following disclaimer in the
- *     documentation and/or other materials provided with the distribution.
- *  3. Neither the name of Sun Microsystems, Inc. nor the names of its
- *     contributors may be used to endorse or promote products derived
- *     from this software without specific prior written permission.
- *
- *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
- *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
- *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
- *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
- *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
- *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
- *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
- *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
- *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
- *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
- *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
- *     
- *************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
+
+
 
 #include <cppuhelper/factory.hxx>
 #include <cppuhelper/servicefactory.hxx>
 #include <cppuhelper/implbase1.hxx>
 #include <cppuhelper/implbase2.hxx>
Index: main/odk/examples/DevelopersGuide/OfficeDev/FilterDevelopment/FlatXmlFilterDetection/filterdetect.cxx
===================================================================
--- main/odk/examples/DevelopersGuide/OfficeDev/FilterDevelopment/FlatXmlFilterDetection/filterdetect.cxx	(revision 1232674)
+++ main/odk/examples/DevelopersGuide/OfficeDev/FilterDevelopment/FlatXmlFilterDetection/filterdetect.cxx	(revision 1239200)
@@ -1,38 +1,27 @@
-/*************************************************************************
- *
- *  The Contents of this file are made available subject to the terms of
- *  the BSD license.
- *  
- *  Copyright 2000, 2010 Oracle and/or its affiliates.
- *  All rights reserved.
- *
- *  Redistribution and use in source and binary forms, with or without
- *  modification, are permitted provided that the following conditions
- *  are met:
- *  1. Redistributions of source code must retain the above copyright
- *     notice, this list of conditions and the following disclaimer.
- *  2. Redistributions in binary form must reproduce the above copyright
- *     notice, this list of conditions and the following disclaimer in the
- *     documentation and/or other materials provided with the distribution.
- *  3. Neither the name of Sun Microsystems, Inc. nor the names of its
- *     contributors may be used to endorse or promote products derived
- *     from this software without specific prior written permission.
- *
- *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
- *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
- *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
- *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
- *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
- *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
- *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
- *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
- *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
- *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
- *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
- *     
- *************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
+
+
 
 #include "filterdetect.hxx"
 #include <osl/diagnose.h>
 #include <com/sun/star/lang/XMultiServiceFactory.hpp>
 #include <com/sun/star/io/XActiveDataSource.hpp>
Index: main/odk/examples/DevelopersGuide/OfficeDev/FilterDevelopment/FlatXmlFilterDetection/fdcomp.cxx
===================================================================
--- main/odk/examples/DevelopersGuide/OfficeDev/FilterDevelopment/FlatXmlFilterDetection/fdcomp.cxx	(revision 1232674)
+++ main/odk/examples/DevelopersGuide/OfficeDev/FilterDevelopment/FlatXmlFilterDetection/fdcomp.cxx	(revision 1239200)
@@ -1,38 +1,27 @@
-/*************************************************************************
- *
- *  The Contents of this file are made available subject to the terms of
- *  the BSD license.
- *  
- *  Copyright 2000, 2010 Oracle and/or its affiliates.
- *  All rights reserved.
- *
- *  Redistribution and use in source and binary forms, with or without
- *  modification, are permitted provided that the following conditions
- *  are met:
- *  1. Redistributions of source code must retain the above copyright
- *     notice, this list of conditions and the following disclaimer.
- *  2. Redistributions in binary form must reproduce the above copyright
- *     notice, this list of conditions and the following disclaimer in the
- *     documentation and/or other materials provided with the distribution.
- *  3. Neither the name of Sun Microsystems, Inc. nor the names of its
- *     contributors may be used to endorse or promote products derived
- *     from this software without specific prior written permission.
- *
- *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
- *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
- *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
- *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
- *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
- *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
- *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
- *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
- *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
- *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
- *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
- *     
- *************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
+
+
 
 #include <stdio.h>
 
 #include <osl/mutex.hxx>
 #include <osl/thread.h>
Index: main/odk/examples/DevelopersGuide/Database/DriverSkeleton/SDriver.cxx
===================================================================
--- main/odk/examples/DevelopersGuide/Database/DriverSkeleton/SDriver.cxx	(revision 1232674)
+++ main/odk/examples/DevelopersGuide/Database/DriverSkeleton/SDriver.cxx	(revision 1239200)
@@ -1,38 +1,27 @@
-/*************************************************************************
- *
- *  The Contents of this file are made available subject to the terms of
- *  the BSD license.
- *  
- *  Copyright 2000, 2010 Oracle and/or its affiliates.
- *  All rights reserved.
- *
- *  Redistribution and use in source and binary forms, with or without
- *  modification, are permitted provided that the following conditions
- *  are met:
- *  1. Redistributions of source code must retain the above copyright
- *     notice, this list of conditions and the following disclaimer.
- *  2. Redistributions in binary form must reproduce the above copyright
- *     notice, this list of conditions and the following disclaimer in the
- *     documentation and/or other materials provided with the distribution.
- *  3. Neither the name of Sun Microsystems, Inc. nor the names of its
- *     contributors may be used to endorse or promote products derived
- *     from this software without specific prior written permission.
- *
- *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
- *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
- *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
- *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
- *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
- *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
- *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
- *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
- *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
- *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
- *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
- *     
- *************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
+
+
 
 #include "SDriver.hxx"
 #include "SConnection.hxx"
 
 using namespace com::sun::star::uno;
Index: main/odk/examples/DevelopersGuide/Database/DriverSkeleton/SResultSetMetaData.cxx
===================================================================
--- main/odk/examples/DevelopersGuide/Database/DriverSkeleton/SResultSetMetaData.cxx	(revision 1232674)
+++ main/odk/examples/DevelopersGuide/Database/DriverSkeleton/SResultSetMetaData.cxx	(revision 1239200)
@@ -1,38 +1,27 @@
-/*************************************************************************
- *
- *  The Contents of this file are made available subject to the terms of
- *  the BSD license.
- *  
- *  Copyright 2000, 2010 Oracle and/or its affiliates.
- *  All rights reserved.
- *
- *  Redistribution and use in source and binary forms, with or without
- *  modification, are permitted provided that the following conditions
- *  are met:
- *  1. Redistributions of source code must retain the above copyright
- *     notice, this list of conditions and the following disclaimer.
- *  2. Redistributions in binary form must reproduce the above copyright
- *     notice, this list of conditions and the following disclaimer in the
- *     documentation and/or other materials provided with the distribution.
- *  3. Neither the name of Sun Microsystems, Inc. nor the names of its
- *     contributors may be used to endorse or promote products derived
- *     from this software without specific prior written permission.
- *
- *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
- *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
- *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
- *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
- *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
- *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
- *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
- *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
- *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
- *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
- *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
- *     
- *************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
+
+
 
 #include "SResultSetMetaData.hxx"
 
 using namespace connectivity::skeleton;
 using namespace com::sun::star::uno;
Index: main/odk/examples/DevelopersGuide/Database/DriverSkeleton/SConnection.cxx
===================================================================
--- main/odk/examples/DevelopersGuide/Database/DriverSkeleton/SConnection.cxx	(revision 1232674)
+++ main/odk/examples/DevelopersGuide/Database/DriverSkeleton/SConnection.cxx	(revision 1239200)
@@ -1,38 +1,27 @@
-/*************************************************************************
- *
- *  The Contents of this file are made available subject to the terms of
- *  the BSD license.
- *  
- *  Copyright 2000, 2010 Oracle and/or its affiliates.
- *  All rights reserved.
- *
- *  Redistribution and use in source and binary forms, with or without
- *  modification, are permitted provided that the following conditions
- *  are met:
- *  1. Redistributions of source code must retain the above copyright
- *     notice, this list of conditions and the following disclaimer.
- *  2. Redistributions in binary form must reproduce the above copyright
- *     notice, this list of conditions and the following disclaimer in the
- *     documentation and/or other materials provided with the distribution.
- *  3. Neither the name of Sun Microsystems, Inc. nor the names of its
- *     contributors may be used to endorse or promote products derived
- *     from this software without specific prior written permission.
- *
- *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
- *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
- *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
- *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
- *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
- *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
- *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
- *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
- *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
- *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
- *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
- *     
- *************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
+
+
 
 #include "SConnection.hxx"
 
 #include "SDatabaseMetaData.hxx"
 #include "SDriver.hxx"
Index: main/odk/examples/DevelopersGuide/Database/DriverSkeleton/propertyids.cxx
===================================================================
--- main/odk/examples/DevelopersGuide/Database/DriverSkeleton/propertyids.cxx	(revision 1232674)
+++ main/odk/examples/DevelopersGuide/Database/DriverSkeleton/propertyids.cxx	(revision 1239200)
@@ -1,38 +1,27 @@
-/*************************************************************************
- *
- *  The Contents of this file are made available subject to the terms of
- *  the BSD license.
- *  
- *  Copyright 2000, 2010 Oracle and/or its affiliates.
- *  All rights reserved.
- *
- *  Redistribution and use in source and binary forms, with or without
- *  modification, are permitted provided that the following conditions
- *  are met:
- *  1. Redistributions of source code must retain the above copyright
- *     notice, this list of conditions and the following disclaimer.
- *  2. Redistributions in binary form must reproduce the above copyright
- *     notice, this list of conditions and the following disclaimer in the
- *     documentation and/or other materials provided with the distribution.
- *  3. Neither the name of Sun Microsystems, Inc. nor the names of its
- *     contributors may be used to endorse or promote products derived
- *     from this software without specific prior written permission.
- *
- *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
- *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
- *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
- *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
- *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
- *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
- *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
- *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
- *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
- *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
- *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
- *     
- *************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
+
+
 
 #include <osl/diagnose.h>
 #include "propertyids.hxx"
 namespace connectivity
 {
Index: main/odk/examples/DevelopersGuide/Database/DriverSkeleton/SStatement.cxx
===================================================================
--- main/odk/examples/DevelopersGuide/Database/DriverSkeleton/SStatement.cxx	(revision 1232674)
+++ main/odk/examples/DevelopersGuide/Database/DriverSkeleton/SStatement.cxx	(revision 1239200)
@@ -1,38 +1,27 @@
-/*************************************************************************
- *
- *  The Contents of this file are made available subject to the terms of
- *  the BSD license.
- *  
- *  Copyright 2000, 2010 Oracle and/or its affiliates.
- *  All rights reserved.
- *
- *  Redistribution and use in source and binary forms, with or without
- *  modification, are permitted provided that the following conditions
- *  are met:
- *  1. Redistributions of source code must retain the above copyright
- *     notice, this list of conditions and the following disclaimer.
- *  2. Redistributions in binary form must reproduce the above copyright
- *     notice, this list of conditions and the following disclaimer in the
- *     documentation and/or other materials provided with the distribution.
- *  3. Neither the name of Sun Microsystems, Inc. nor the names of its
- *     contributors may be used to endorse or promote products derived
- *     from this software without specific prior written permission.
- *
- *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
- *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
- *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
- *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
- *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
- *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
- *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
- *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
- *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
- *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
- *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
- *     
- *************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
+
+
 
 #include <stdio.h>
 #include <osl/diagnose.h>
 #include "SStatement.hxx"
 #include "SConnection.hxx"
Index: main/odk/examples/DevelopersGuide/Database/DriverSkeleton/SDatabaseMetaData.cxx
===================================================================
--- main/odk/examples/DevelopersGuide/Database/DriverSkeleton/SDatabaseMetaData.cxx	(revision 1232674)
+++ main/odk/examples/DevelopersGuide/Database/DriverSkeleton/SDatabaseMetaData.cxx	(revision 1239200)
@@ -1,38 +1,27 @@
-/*************************************************************************
- *
- *  The Contents of this file are made available subject to the terms of
- *  the BSD license.
- *  
- *  Copyright 2000, 2010 Oracle and/or its affiliates.
- *  All rights reserved.
- *
- *  Redistribution and use in source and binary forms, with or without
- *  modification, are permitted provided that the following conditions
- *  are met:
- *  1. Redistributions of source code must retain the above copyright
- *     notice, this list of conditions and the following disclaimer.
- *  2. Redistributions in binary form must reproduce the above copyright
- *     notice, this list of conditions and the following disclaimer in the
- *     documentation and/or other materials provided with the distribution.
- *  3. Neither the name of Sun Microsystems, Inc. nor the names of its
- *     contributors may be used to endorse or promote products derived
- *     from this software without specific prior written permission.
- *
- *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
- *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
- *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
- *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
- *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
- *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
- *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
- *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
- *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
- *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
- *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
- *     
- *************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
+
+
 
 #include "SDatabaseMetaData.hxx"
 #include <com/sun/star/sdbc/DataType.hpp>
 #include <com/sun/star/sdbc/ResultSetType.hpp>
 #include <com/sun/star/sdbc/ResultSetConcurrency.hpp>
Index: main/odk/examples/DevelopersGuide/Database/DriverSkeleton/SServices.cxx
===================================================================
--- main/odk/examples/DevelopersGuide/Database/DriverSkeleton/SServices.cxx	(revision 1232674)
+++ main/odk/examples/DevelopersGuide/Database/DriverSkeleton/SServices.cxx	(revision 1239200)
@@ -1,38 +1,27 @@
-/*************************************************************************
- *
- *  The Contents of this file are made available subject to the terms of
- *  the BSD license.
- *  
- *  Copyright 2000, 2010 Oracle and/or its affiliates.
- *  All rights reserved.
- *
- *  Redistribution and use in source and binary forms, with or without
- *  modification, are permitted provided that the following conditions
- *  are met:
- *  1. Redistributions of source code must retain the above copyright
- *     notice, this list of conditions and the following disclaimer.
- *  2. Redistributions in binary form must reproduce the above copyright
- *     notice, this list of conditions and the following disclaimer in the
- *     documentation and/or other materials provided with the distribution.
- *  3. Neither the name of Sun Microsystems, Inc. nor the names of its
- *     contributors may be used to endorse or promote products derived
- *     from this software without specific prior written permission.
- *
- *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
- *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
- *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
- *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
- *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
- *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
- *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
- *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
- *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
- *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
- *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
- *     
- *************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
+
+
 
 #include <sal/types.h>
 #include "SDriver.hxx"
 #include <cppuhelper/factory.hxx>
 #include <osl/diagnose.h>
Index: main/odk/examples/DevelopersGuide/Database/DriverSkeleton/SPreparedStatement.cxx
===================================================================
--- main/odk/examples/DevelopersGuide/Database/DriverSkeleton/SPreparedStatement.cxx	(revision 1232674)
+++ main/odk/examples/DevelopersGuide/Database/DriverSkeleton/SPreparedStatement.cxx	(revision 1239200)
@@ -1,38 +1,27 @@
-/*************************************************************************
- *
- *  The Contents of this file are made available subject to the terms of
- *  the BSD license.
- *  
- *  Copyright 2000, 2010 Oracle and/or its affiliates.
- *  All rights reserved.
- *
- *  Redistribution and use in source and binary forms, with or without
- *  modification, are permitted provided that the following conditions
- *  are met:
- *  1. Redistributions of source code must retain the above copyright
- *     notice, this list of conditions and the following disclaimer.
- *  2. Redistributions in binary form must reproduce the above copyright
- *     notice, this list of conditions and the following disclaimer in the
- *     documentation and/or other materials provided with the distribution.
- *  3. Neither the name of Sun Microsystems, Inc. nor the names of its
- *     contributors may be used to endorse or promote products derived
- *     from this software without specific prior written permission.
- *
- *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
- *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
- *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
- *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
- *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
- *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
- *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
- *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
- *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
- *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
- *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
- *     
- *************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
+
+
 
 #include <stdio.h>
 #include <osl/diagnose.h>
 #include "SPreparedStatement.hxx"
 #include <com/sun/star/sdbc/DataType.hpp>
Index: main/odk/examples/DevelopersGuide/Database/DriverSkeleton/SResultSet.cxx
===================================================================
--- main/odk/examples/DevelopersGuide/Database/DriverSkeleton/SResultSet.cxx	(revision 1232674)
+++ main/odk/examples/DevelopersGuide/Database/DriverSkeleton/SResultSet.cxx	(revision 1239200)
@@ -1,38 +1,27 @@
-/*************************************************************************
- *
- *  The Contents of this file are made available subject to the terms of
- *  the BSD license.
- *  
- *  Copyright 2000, 2010 Oracle and/or its affiliates.
- *  All rights reserved.
- *
- *  Redistribution and use in source and binary forms, with or without
- *  modification, are permitted provided that the following conditions
- *  are met:
- *  1. Redistributions of source code must retain the above copyright
- *     notice, this list of conditions and the following disclaimer.
- *  2. Redistributions in binary form must reproduce the above copyright
- *     notice, this list of conditions and the following disclaimer in the
- *     documentation and/or other materials provided with the distribution.
- *  3. Neither the name of Sun Microsystems, Inc. nor the names of its
- *     contributors may be used to endorse or promote products derived
- *     from this software without specific prior written permission.
- *
- *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
- *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
- *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
- *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
- *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
- *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
- *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
- *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
- *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
- *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
- *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
- *     
- *************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
+
+
 
 #include "SResultSet.hxx"
 #include "SResultSetMetaData.hxx"
 #include <com/sun/star/sdbc/DataType.hpp>
 #include <com/sun/star/beans/PropertyAttribute.hpp>
Index: main/unodevtools/source/skeletonmaker/skeletoncommon.cxx
===================================================================
--- main/unodevtools/source/skeletonmaker/skeletoncommon.cxx	(revision 1232674)
+++ main/unodevtools/source/skeletonmaker/skeletoncommon.cxx	(revision 1239200)
@@ -65,11 +65,11 @@
         " * \"AS IS\" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY\n"
         " * KIND, either express or implied.  See the License for the\n"
         " * specific language governing permissions and limitations\n"
         " * under the License.\n"
         " * \n"
-        " *************************************************************/\n\n"
+        " *************************************************************/\n\n";
 }
 
 bool getOutputStream(ProgramOptions const & options,
                      OString const & extension,
                      std::ostream** ppOutputStream,
Index: main/svgio/source/svgreader/svgcharacternode.cxx
===================================================================
--- main/svgio/source/svgreader/svgcharacternode.cxx	(revision 1232674)
+++ main/svgio/source/svgreader/svgcharacternode.cxx	(revision 1239200)
@@ -175,13 +175,13 @@
             /// does nothing.
             virtual bool allowChange(sal_uInt32 nCount, basegfx::B2DHomMatrix& rNewTransform, sal_uInt32 nIndex, sal_uInt32 nLength);
 
         public:
             localTextBreakupHelper(
-                const drawinglayer::primitive2d::Primitive2DReference& rxSource, 
+                const drawinglayer::primitive2d::TextSimplePortionPrimitive2D& rSource, 
                 SvgTextPosition& rSvgTextPosition)
-            :   drawinglayer::primitive2d::TextBreakupHelper(rxSource),
+            :   drawinglayer::primitive2d::TextBreakupHelper(rSource),
                 mrSvgTextPosition(rSvgTextPosition)
             {
             }
         };
 
@@ -484,21 +484,31 @@
                     drawinglayer::primitive2d::appendPrimitive2DReferenceToPrimitive2DSequence(rTarget, xRef);
                 }
                 else
                 {
                     // need to apply rotations to each character as given
-                    localTextBreakupHelper alocalTextBreakupHelper(xRef, rSvgTextPosition);
-                    const drawinglayer::primitive2d::Primitive2DSequence aResult(
-                        alocalTextBreakupHelper.getResult(drawinglayer::primitive2d::BreakupUnit_character));
+                    const drawinglayer::primitive2d::TextSimplePortionPrimitive2D* pCandidate = 
+                        dynamic_cast< const drawinglayer::primitive2d::TextSimplePortionPrimitive2D* >(xRef.get());
 
-                    if(aResult.hasElements())
+                    if(pCandidate)
                     {
-                        drawinglayer::primitive2d::appendPrimitive2DSequenceToPrimitive2DSequence(rTarget, aResult);
-                    }
+                        const localTextBreakupHelper alocalTextBreakupHelper(*pCandidate, rSvgTextPosition);
+                        const drawinglayer::primitive2d::Primitive2DSequence aResult(
+                            alocalTextBreakupHelper.getResult(drawinglayer::primitive2d::BreakupUnit_character));
+
+                        if(aResult.hasElements())
+                        {
+                            drawinglayer::primitive2d::appendPrimitive2DSequenceToPrimitive2DSequence(rTarget, aResult);
+                        }
 
-                    // also consume for the implied single space
-                    rSvgTextPosition.consumeRotation();
+                        // also consume for the implied single space
+                        rSvgTextPosition.consumeRotation();
+                    }
+                    else
+                    {
+                        OSL_ENSURE(false, "Used primitive is not a text primitive (!)");
+                    }
                 }
             }
         }
 
         void SvgCharacterNode::whiteSpaceHandling()
Index: main/svgio/source/svgreader/svgtextpathnode.cxx
===================================================================
--- main/svgio/source/svgreader/svgtextpathnode.cxx	(revision 1232674)
+++ main/svgio/source/svgreader/svgtextpathnode.cxx	(revision 1239200)
@@ -65,11 +65,11 @@
             basegfx::B2DCubicBezierHelper* getB2DCubicBezierHelper();
             void advanceToPosition(double fNewPosition);
 
         public:
             pathTextBreakupHelper(
-                const drawinglayer::primitive2d::Primitive2DReference& rxSource,
+                const drawinglayer::primitive2d::TextSimplePortionPrimitive2D& rSource,
                 const basegfx::B2DPolygon& rPolygon,
                 const double fBasegfxPathLength,
                 const double fUserToBasegfx,
                 double fPosition,
                 const basegfx::B2DPoint& rTextStart);
@@ -133,17 +133,17 @@
 
             mfPosition = fNewPosition;
         }
 
         pathTextBreakupHelper::pathTextBreakupHelper(
-            const drawinglayer::primitive2d::Primitive2DReference& rxSource,
+            const drawinglayer::primitive2d::TextSimplePortionPrimitive2D& rSource,
             const basegfx::B2DPolygon& rPolygon,
             const double fBasegfxPathLength,
             const double fUserToBasegfx,
             double fPosition,
             const basegfx::B2DPoint& rTextStart)
-        :   drawinglayer::primitive2d::TextBreakupHelper(rxSource),
+        :   drawinglayer::primitive2d::TextBreakupHelper(rSource),
             mrPolygon(rPolygon),
             mfBasegfxPathLength(fBasegfxPathLength),
             mfUserToBasegfx(fUserToBasegfx),
             mfPosition(0.0),
             mrTextStart(rTextStart),
@@ -167,21 +167,21 @@
 
         bool pathTextBreakupHelper::allowChange(sal_uInt32 /*nCount*/, basegfx::B2DHomMatrix& rNewTransform, sal_uInt32 nIndex, sal_uInt32 nLength)
         {
             bool bRetval(false);
 
-            if(mfPosition < mfBasegfxPathLength && nLength && getCastedSource() && mnIndex < mnMaxIndex)
+            if(mfPosition < mfBasegfxPathLength && nLength && mnIndex < mnMaxIndex)
             {
                 const double fSnippetWidth(
                     getTextLayouter().getTextWidth(
-                        getCastedSource()->getText(),
+                        getSource().getText(),
                         nIndex,
                         nLength));
 
                 if(basegfx::fTools::more(fSnippetWidth, 0.0))
                 {
-                    const ::rtl::OUString aText(getCastedSource()->getText());
+                    const ::rtl::OUString aText(getSource().getText());
                     const ::rtl::OUString aTrimmedChars(aText.copy(nIndex, nLength).trim());
                     const double fEndPos(mfPosition + fSnippetWidth);
 
                     if(aTrimmedChars.getLength() && (mfPosition < mfBasegfxPathLength || fEndPos > 0.0))
                     {
@@ -476,12 +476,12 @@
                                         pCandidate = dynamic_cast< const drawinglayer::primitive2d::TextSimplePortionPrimitive2D* >(xReference.get());
                                     }
 
                                     if(pCandidate)
                                     {
-                                        pathTextBreakupHelper aPathTextBreakupHelper(
-                                            xReference,
+                                        const pathTextBreakupHelper aPathTextBreakupHelper(
+                                            *pCandidate,
                                             aPolygon,
                                             fBasegfxPathLength,
                                             fUserToBasegfx,
                                             fPosition,
                                             rTextStart);
Index: main/svgio/source/svgreader/svgstyleattributes.cxx
===================================================================
--- main/svgio/source/svgreader/svgstyleattributes.cxx	(revision 1232674)
+++ main/svgio/source/svgreader/svgstyleattributes.cxx	(revision 1239200)
@@ -40,10 +40,12 @@
 #include <basegfx/polygon/b2dpolypolygontools.hxx>
 #include <svgio/svgreader/svgmarkernode.hxx>
 #include <basegfx/curve/b2dcubicbezier.hxx>
 #include <svgio/svgreader/svgpatternnode.hxx>
 #include <drawinglayer/primitive2d/patternfillprimitive2d.hxx>
+#include <basegfx/polygon/b2dpolygontools.hxx>
+#include <drawinglayer/primitive2d/maskprimitive2d.hxx>
 
 //////////////////////////////////////////////////////////////////////////////
 
 namespace svgio
 {
@@ -847,66 +849,77 @@
             // get marker primitive representation
             rMarkerPrimitives = rMarker.getMarkerPrimitives();
 
             if(rMarkerPrimitives.hasElements())
             {
-                basegfx::B2DRange aPrimitiveRange;
+                basegfx::B2DRange aPrimitiveRange(0.0, 0.0, 1.0, 1.0);
+                const basegfx::B2DRange* pViewBox = rMarker.getViewBox();
 
-                if(rMarker.getViewBox())
+                if(pViewBox)
                 {
-                    aPrimitiveRange = *rMarker.getViewBox();
-                }
-                else
-                {
-                    aPrimitiveRange = drawinglayer::primitive2d::getB2DRangeFromPrimitive2DSequence(
-                        rMarkerPrimitives,
-                        drawinglayer::geometry::ViewInformation2D());
+                    aPrimitiveRange = *pViewBox;
                 }
 
                 if(aPrimitiveRange.getWidth() > 0.0 && aPrimitiveRange.getHeight() > 0.0)
                 {
-                    double fTargetWidth(rMarker.getMarkerWidth().isSet() ? rMarker.getMarkerWidth().solve(mrOwner, xcoordinate) : 0.0);
-                    double fTargetHeight(rMarker.getMarkerHeight().isSet() ? rMarker.getMarkerHeight().solve(mrOwner, xcoordinate) : 0.0);
+                    double fTargetWidth(rMarker.getMarkerWidth().isSet() ? rMarker.getMarkerWidth().solve(mrOwner, xcoordinate) : 3.0);
+                    double fTargetHeight(rMarker.getMarkerHeight().isSet() ? rMarker.getMarkerHeight().solve(mrOwner, xcoordinate) : 3.0);
+                    const bool bStrokeWidth(SvgMarkerNode::strokeWidth == rMarker.getMarkerUnits());
+                    const double fStrokeWidth(getStrokeWidth().isSet() ? getStrokeWidth().solve(mrOwner, length) : 1.0);
 
-                    if(SvgMarkerNode::strokeWidth == rMarker.getMarkerUnits())
+                    if(bStrokeWidth)
                     {
                         // relative to strokeWidth
-                        const double fStrokeWidth(getStrokeWidth().isSet() ? getStrokeWidth().solve(mrOwner, length) : 1.0);
-
                         fTargetWidth *= fStrokeWidth;
                         fTargetHeight *= fStrokeWidth;
                     }
 
                     if(fTargetWidth > 0.0 && fTargetHeight > 0.0)
                     {
-                        const basegfx::B2DRange aTargetRange(0.0, 0.0, fTargetWidth, fTargetHeight);
-
-                        // subbstract refX, refY first, it's in marker local coordinates
-                        rMarkerTransform.translate(
-                            rMarker.getRefX().isSet() ? -rMarker.getRefX().solve(mrOwner, xcoordinate) : 0.0,
-                            rMarker.getRefY().isSet() ? -rMarker.getRefY().solve(mrOwner, ycoordinate) : 0.0);
-
                         // create mapping
+                        const basegfx::B2DRange aTargetRange(0.0, 0.0, fTargetWidth, fTargetHeight);
                         const SvgAspectRatio& rRatio = rMarker.getSvgAspectRatio();
 
                         if(rRatio.isSet())
                         {
                             // let mapping be created from SvgAspectRatio
-                            rMarkerTransform = rRatio.createMapping(aTargetRange, aPrimitiveRange) * rMarkerTransform;
+                            rMarkerTransform = rRatio.createMapping(aTargetRange, aPrimitiveRange);
                             
                             if(rRatio.isMeetOrSlice())
                             {
                                 // need to clip
-                                rClipRange = basegfx::B2DRange(0.0, 0.0, 1.0, 1.0);
+                                rClipRange = aPrimitiveRange;
                             }
                         }
                         else
                         {
-                            // choose default mapping
-                            rMarkerTransform = rRatio.createLinearMapping(aTargetRange, aPrimitiveRange) * rMarkerTransform;
+                            if(!pViewBox)
+                            {
+                                if(bStrokeWidth)
+                                {
+                                    // adapt to strokewidth if needed
+                                    rMarkerTransform.scale(fStrokeWidth, fStrokeWidth);
+                                }
+                            }
+                            else
+                            {
+                                // choose default mapping
+                                rMarkerTransform = rRatio.createLinearMapping(aTargetRange, aPrimitiveRange);
+                            }
                         }
 
+                        // get and apply reference point. Initially it's in marker local coordinate system
+                        basegfx::B2DPoint aRefPoint(
+                            rMarker.getRefX().isSet() ? rMarker.getRefX().solve(mrOwner, xcoordinate) : 0.0,
+                            rMarker.getRefY().isSet() ? rMarker.getRefY().solve(mrOwner, ycoordinate) : 0.0);
+
+                        // apply MarkerTransform to have it in mapped coordinates
+                        aRefPoint *= rMarkerTransform;
+
+                        // apply by moving RepPoint to (0.0)
+                        rMarkerTransform.translate(-aRefPoint.getX(), -aRefPoint.getY());
+
                         return true;
                     }
                 }
             }
 
@@ -915,11 +928,11 @@
 
         void SvgStyleAttributes::add_singleMarker(
             drawinglayer::primitive2d::Primitive2DSequence& rTarget,
             const drawinglayer::primitive2d::Primitive2DSequence& rMarkerPrimitives,
             const basegfx::B2DHomMatrix& rMarkerTransform,
-            const basegfx::B2DRange& /*rClipRange*/,
+            const basegfx::B2DRange& rClipRange,
             const SvgMarkerNode& rMarker,
             const basegfx::B2DPolygon& rCandidate,
             const sal_uInt32 nIndex) const
         {
             const sal_uInt32 nPointCount(rCandidate.count());
@@ -932,16 +945,29 @@
 
                 // get and apply target position
                 const basegfx::B2DPoint aPoint(rCandidate.getB2DPoint(nIndex % nPointCount));
                 aCombinedTransform.translate(aPoint.getX(), aPoint.getY());
 
-                // add marker
-                drawinglayer::primitive2d::appendPrimitive2DReferenceToPrimitive2DSequence(
-                    rTarget,
+                // prepare marker
+                drawinglayer::primitive2d::Primitive2DReference xMarker(
                     new drawinglayer::primitive2d::TransformPrimitive2D(
                         aCombinedTransform,
                         rMarkerPrimitives));
+
+                if(!rClipRange.isEmpty())
+                {
+                    // marker needs to be clipped, it's bigger as the mapping
+                    basegfx::B2DPolyPolygon aClipPolygon(basegfx::tools::createPolygonFromRect(rClipRange));
+
+                    aClipPolygon.transform(aCombinedTransform);
+                    xMarker = new drawinglayer::primitive2d::MaskPrimitive2D(
+                        aClipPolygon,
+                        drawinglayer::primitive2d::Primitive2DSequence(&xMarker, 1));
+                }
+
+                // add marker
+                drawinglayer::primitive2d::appendPrimitive2DReferenceToPrimitive2DSequence(rTarget, xMarker);
             }
         }
 
         void SvgStyleAttributes::add_markers(
             const basegfx::B2DPolyPolygon& rPath,
Index: main/basegfx/source/polygon/b2dlinegeometry.cxx
===================================================================
--- main/basegfx/source/polygon/b2dlinegeometry.cxx	(revision 1232674)
+++ main/basegfx/source/polygon/b2dlinegeometry.cxx	(revision 1239200)
@@ -33,10 +33,11 @@
 #include <basegfx/range/b2drange.hxx>
 #include <basegfx/matrix/b2dhommatrix.hxx>
 #include <basegfx/curve/b2dcubicbezier.hxx>
 #include <basegfx/matrix/b2dhommatrixtools.hxx>
 #include <com/sun/star/drawing/LineCap.hpp>
+#include <basegfx/polygon/b2dpolypolygoncutter.hxx>
 
 //////////////////////////////////////////////////////////////////////////////
 
 namespace basegfx
 {
@@ -337,11 +338,11 @@
             {
                 return rCandidate;
             }
         }
 
-        B2DPolyPolygon createAreaGeometryForEdge(
+        B2DPolygon createAreaGeometryForEdge(
             const B2DCubicBezier& rEdge, 
             double fHalfLineWidth,
             bool bStartRound,
             bool bEndRound,
             bool bStartSquare,
@@ -352,11 +353,10 @@
             // the in-between points EdgeEnd and EdgeStart, it leads to rounding
             // errors when converting to integer polygon coordinates for painting
             if(rEdge.isBezier())
             {
                 // prepare target and data common for upper and lower
-                B2DPolyPolygon aRetval;
                 B2DPolygon aBezierPolygon;
                 const B2DVector aPureEdgeVector(rEdge.getEndPoint() - rEdge.getStartPoint());
                 const double fEdgeLength(aPureEdgeVector.getLength());
                 const bool bIsEdgeLengthZero(fTools::equalZero(fEdgeLength));
                 B2DVector aTangentA(rEdge.getTangent(0.0)); aTangentA.normalize();
@@ -384,54 +384,44 @@
                     CUTFLAG_ALL, &fCutB));
                 const bool bCutB(CUTFLAG_NONE != aCutB);
 
                 // check if cut happens
                 const bool bCut(bCutA || bCutB);
+                B2DPoint aCutPoint;
 
                 // create left edge
                 if(bStartRound || bStartSquare)
                 {
-                    basegfx::B2DPolygon aStartPolygon;
-
                     if(bStartRound)
                     {
-                        aStartPolygon = tools::createHalfUnitCircle();
+                        basegfx::B2DPolygon aStartPolygon(tools::createHalfUnitCircle());
+
                         aStartPolygon.transform(
                             tools::createScaleShearXRotateTranslateB2DHomMatrix(
                                 fHalfLineWidth, fHalfLineWidth,
                                 0.0,
                                 atan2(aTangentA.getY(), aTangentA.getX()) + F_PI2,
                                 rEdge.getStartPoint().getX(), rEdge.getStartPoint().getY()));
+                        aBezierPolygon.append(aStartPolygon);
                     }
                     else // bStartSquare
                     {
                         const basegfx::B2DPoint aStart(rEdge.getStartPoint() - (aTangentA * fHalfLineWidth));
 
-                        if(bCut)
+                        if(bCutB)
                         {
-                            aStartPolygon.append(rEdge.getStartPoint() + aPerpendStartB);
+                            aBezierPolygon.append(rEdge.getStartPoint() + aPerpendStartB);
                         }
 
-                        aStartPolygon.append(aStart + aPerpendStartB);
-                        aStartPolygon.append(aStart + aPerpendStartA);
+                        aBezierPolygon.append(aStart + aPerpendStartB);
+                        aBezierPolygon.append(aStart + aPerpendStartA);
 
-                        if(bCut)
+                        if(bCutA)
                         {
-                            aStartPolygon.append(rEdge.getStartPoint() + aPerpendStartA);
+                            aBezierPolygon.append(rEdge.getStartPoint() + aPerpendStartA);
                         }
                     }
-
-                    if(bCut)
-                    {
-                        aStartPolygon.append(rEdge.getStartPoint());
-                        aStartPolygon.setClosed(true);
-                        aRetval.append(aStartPolygon);
-                    }
-                    else
-                    {
-                        aBezierPolygon.append(aStartPolygon);
-                    }
                 }
                 else
                 {
                     // append original in-between point
                     aBezierPolygon.append(rEdge.getStartPoint());
@@ -440,11 +430,11 @@
                 // create upper edge.
                 {
                     if(bCutA)
                     {
                         // calculate cut point and add
-                        const B2DPoint aCutPoint(rEdge.getStartPoint() + (aPerpendStartA * fCutA));
+                        aCutPoint = rEdge.getStartPoint() + (aPerpendStartA * fCutA);
                         aBezierPolygon.append(aCutPoint);
                     }
                     else
                     {
                         // create scaled bezier segment
@@ -462,50 +452,39 @@
                 }
 
                 // create right edge
                 if(bEndRound || bEndSquare)
                 {
-                    basegfx::B2DPolygon aEndPolygon;
-
                     if(bEndRound)
                     {
-                        aEndPolygon = tools::createHalfUnitCircle();
+                        basegfx::B2DPolygon aEndPolygon(tools::createHalfUnitCircle());
+
                         aEndPolygon.transform(
                             tools::createScaleShearXRotateTranslateB2DHomMatrix(
                                 fHalfLineWidth, fHalfLineWidth,
                                 0.0,
                                 atan2(aTangentB.getY(), aTangentB.getX()) - F_PI2,
                                 rEdge.getEndPoint().getX(), rEdge.getEndPoint().getY()));
+                        aBezierPolygon.append(aEndPolygon);
                     }
                     else // bEndSquare
                     {
                         const basegfx::B2DPoint aEnd(rEdge.getEndPoint() + (aTangentB * fHalfLineWidth));
 
-                        if(bCut)
+                        if(bCutA)
                         {
-                            aEndPolygon.append(rEdge.getEndPoint() + aPerpendEndA);
+                            aBezierPolygon.append(rEdge.getEndPoint() + aPerpendEndA);
                         }
 
-                        aEndPolygon.append(aEnd + aPerpendEndA);
-                        aEndPolygon.append(aEnd + aPerpendEndB);
+                        aBezierPolygon.append(aEnd + aPerpendEndA);
+                        aBezierPolygon.append(aEnd + aPerpendEndB);
 
-                        if(bCut)
+                        if(bCutB)
                         {
-                            aEndPolygon.append(rEdge.getEndPoint() + aPerpendEndB);
+                            aBezierPolygon.append(rEdge.getEndPoint() + aPerpendEndB);
                         }
                     }
-
-                    if(bCut)
-                    {
-                        aEndPolygon.append(rEdge.getEndPoint());
-                        aEndPolygon.setClosed(true);
-                        aRetval.append(aEndPolygon);
-                    }
-                    else
-                    {
-                        aBezierPolygon.append(aEndPolygon);
-                    }
                 }
                 else
                 {
                     // append original in-between point
                     aBezierPolygon.append(rEdge.getEndPoint());
@@ -514,11 +493,11 @@
                 // create lower edge. 
                 {
                     if(bCutB)
                     {
                         // calculate cut point and add
-                        const B2DPoint aCutPoint(rEdge.getEndPoint() + (aPerpendEndB * fCutB));
+                        aCutPoint = rEdge.getEndPoint() + (aPerpendEndB * fCutB);
                         aBezierPolygon.append(aCutPoint);
                     }
                     else
                     {
                         // create scaled bezier segment
@@ -533,15 +512,73 @@
                         aBezierPolygon.append(aStart);
                         aBezierPolygon.appendBezierSegment(aStart + (fRelNext * fScale), aEnd + (fRelPrev * fScale), aEnd);
                     }
                 }
 
-                // close and return
+                // close
                 aBezierPolygon.setClosed(true);
-                aRetval.append(aBezierPolygon);
-                
-                return aRetval;
+
+                if(bStartRound || bEndRound)
+                {
+                    // double points possible when round caps are used at start or end
+                    aBezierPolygon.removeDoublePoints();
+                }
+
+                if(bCut && ((bStartRound || bStartSquare) && (bEndRound || bEndSquare)))
+                {
+                    // When cut exists and both ends are extended with caps, a self-intersecting polygon
+                    // is created; one cut point is known, but there is a 2nd one in the caps geometry.
+                    // Solve by using tooling.
+                    // Remark: This nearly never happens due to curve preparations to extreme points
+                    // and maximum angle turning, but I constructed a test case and checkd that it is 
+                    // working propery.
+                    const B2DPolyPolygon aTemp(tools::solveCrossovers(aBezierPolygon));
+                    const sal_uInt32 nTempCount(aTemp.count());
+
+                    if(nTempCount)
+                    {
+                        if(nTempCount > 1)
+                        {
+                            // as expected, multiple polygons (with same orientation). Remove
+                            // the one which contains aCutPoint, or better take the one without
+                            for (sal_uInt32 a(0); a < nTempCount; a++)
+                            {
+                                aBezierPolygon = aTemp.getB2DPolygon(a);
+
+                                const sal_uInt32 nCandCount(aBezierPolygon.count());
+
+                                for(sal_uInt32 b(0); b < nCandCount; b++)
+                                {
+                                    if(aCutPoint.equal(aBezierPolygon.getB2DPoint(b)))
+                                    {
+                                        aBezierPolygon.clear();
+                                        break;
+                                    }
+                                }
+
+                                if(aBezierPolygon.count())
+                                {
+                                    break;
+                                }
+                            }
+
+                            OSL_ENSURE(aBezierPolygon.count(), "Error in line geometry creation, could not solve self-intersection (!)");
+                        }
+                        else
+                        {
+                            // none found, use result
+                            aBezierPolygon = aTemp.getB2DPolygon(0);
+                        }
+                    }
+                    else
+                    {
+                        OSL_ENSURE(false, "Error in line geometry creation, could not solve self-intersection (!)");
+                    }
+                }
+
+                // return
+                return aBezierPolygon;
             }
             else
             {
                 // Get start and  end point, create tangent and set to needed length
                 B2DVector aTangent(rEdge.getEndPoint() - rEdge.getStartPoint());
@@ -634,11 +671,11 @@
                 }
 
                 // close and return
                 aEdgePolygon.setClosed(true);
 
-                return B2DPolyPolygon(aEdgePolygon);
+                return aEdgePolygon;
             }
         }
 
         B2DPolygon createAreaGeometryForJoin(
             const B2DVector& rTangentPrev, 
Index: main/svx/source/xoutdev/xattr2.cxx
===================================================================
--- main/svx/source/xoutdev/xattr2.cxx	(revision 1232674)
+++ main/svx/source/xoutdev/xattr2.cxx	(revision 1239200)
@@ -449,11 +449,11 @@
     const com::sun::star::drawing::LineCap eRetval((com::sun::star::drawing::LineCap)SfxEnumItem::GetValue());
     OSL_ENSURE(com::sun::star::drawing::LineCap_BUTT == eRetval
         || com::sun::star::drawing::LineCap_ROUND == eRetval
         || com::sun::star::drawing::LineCap_SQUARE == eRetval, "Unknown enum value in XATTR_LINECAP (!)");
 
-    return (com::sun::star::drawing::LineCap)SfxEnumItem::GetValue(); 
+    return eRetval; 
 }
 
 //------------------------------
 // class XFillTransparenceItem
 //------------------------------
Index: main/svx/source/svdraw/svdfmtf.cxx
===================================================================
--- main/svx/source/svdraw/svdfmtf.cxx	(revision 1232674)
+++ main/svx/source/svdraw/svdfmtf.cxx	(revision 1239200)
@@ -359,11 +359,11 @@
         // #i118485# Setting this item leads to problems (written #i118498# for this)
         // pTextAttr->Put(SvxAutoKernItem(aFnt.IsKerning(), EE_CHAR_KERNING));
 
         pTextAttr->Put(SvxWordLineModeItem(aFnt.IsWordLineMode(), EE_CHAR_WLM));
         pTextAttr->Put(SvxContourItem(aFnt.IsOutline(), EE_CHAR_OUTLINE));
-        pTextAttr->Put(SvxColorItem(aFnt.GetColor(), EE_CHAR_COLOR));
+        pTextAttr->Put(SvxColorItem(aVD.GetTextColor(), EE_CHAR_COLOR));
 		//... svxfont textitem svditext
 		bFntDirty=sal_False;
 	}
 	if (pObj!=NULL)
 	{
Index: main/filter/source/svg/svgdialog.cxx
===================================================================
--- main/filter/source/svg/svgdialog.cxx	(revision 1232674)
+++ main/filter/source/svg/svgdialog.cxx	(revision 1239200)
@@ -1,39 +1,26 @@
- /*************************************************************************
- *
- *  OpenOffice.org - a multi-platform office productivity suite
- *
- *  $RCSfile: svgdialog.cxx,v $
- *
- *  $Revision: 1.1.2.3 $
- *
- *  last change: $Author: ka $ $Date: 2008/05/19 10:12:43 $
- *
- *  The Contents of this file are made available subject to
- *  the terms of GNU Lesser General Public License Version 2.1.
- *
- *
- *    GNU Lesser General Public License Version 2.1
- *    =============================================
- *    Copyright 2005 by Sun Microsystems, Inc.
- *    901 San Antonio Road, Palo Alto, CA 94303, USA
- *
- *    This library is free software; you can redistribute it and/or
- *    modify it under the terms of the GNU Lesser General Public
- *    License version 2.1, as published by the Free Software Foundation.
- *
- *    This library is distributed in the hope that it will be useful,
- *    but WITHOUT ANY WARRANTY; without even the implied warranty of
- *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
- *    Lesser General Public License for more details.
- *
- *    You should have received a copy of the GNU Lesser General Public
- *    License along with this library; if not, write to the Free Software
- *    Foundation, Inc., 59 Temple Place, Suite 330, Boston,
- *    MA  02111-1307  USA
- *
- ************************************************************************/
+ /**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
+
 // MARKER(update_precomp.py): autogen include statement, do not remove
 #include "precompiled_filter.hxx"
 
 #include "svgdialog.hxx" 
 #include "impsvgdialog.hxx" 
Index: main/filter/source/svg/impsvgdialog.cxx
===================================================================
--- main/filter/source/svg/impsvgdialog.cxx	(revision 1232674)
+++ main/filter/source/svg/impsvgdialog.cxx	(revision 1239200)
@@ -1,39 +1,25 @@
- /*************************************************************************
- *
- *  OpenOffice.org - a multi-platform office productivity suite
- *
- *  $RCSfile: impsvgdialog.cxx,v $
- *
- *  $Revision: 1.1.2.3 $
- *
- *  last change: $Author: ka $ $Date: 2007/06/15 14:36:19 $
- *
- *  The Contents of this file are made available subject to
- *  the terms of GNU Lesser General Public License Version 2.1.
- *
- *
- *    GNU Lesser General Public License Version 2.1
- *    =============================================
- *    Copyright 2005 by Sun Microsystems, Inc.
- *    901 San Antonio Road, Palo Alto, CA 94303, USA
- *
- *    This library is free software; you can redistribute it and/or
- *    modify it under the terms of the GNU Lesser General Public
- *    License version 2.1, as published by the Free Software Foundation.
- *
- *    This library is distributed in the hope that it will be useful,
- *    but WITHOUT ANY WARRANTY; without even the implied warranty of
- *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
- *    Lesser General Public License for more details.
- *
- *    You should have received a copy of the GNU Lesser General Public
- *    License along with this library; if not, write to the Free Software
- *    Foundation, Inc., 59 Temple Place, Suite 330, Boston,
- *    MA  02111-1307  USA
- *
- ************************************************************************/
+/**************************************************************
+ * 
+ * Licensed to the Apache Software Foundation (ASF) under one
+ * or more contributor license agreements.  See the NOTICE file
+ * distributed with this work for additional information
+ * regarding copyright ownership.  The ASF licenses this file
+ * to you under the Apache License, Version 2.0 (the
+ * "License"); you may not use this file except in compliance
+ * with the License.  You may obtain a copy of the License at
+ * 
+ *   http://www.apache.org/licenses/LICENSE-2.0
+ * 
+ * Unless required by applicable law or agreed to in writing,
+ * software distributed under the License is distributed on an
+ * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
+ * KIND, either express or implied.  See the License for the
+ * specific language governing permissions and limitations
+ * under the License.
+ * 
+ *************************************************************/
 
 // MARKER(update_precomp.py): autogen include statement, do not remove
 #include "precompiled_filter.hxx"
 
 #include "impsvgdialog.hxx"
