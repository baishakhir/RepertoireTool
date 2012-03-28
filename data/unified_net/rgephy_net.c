Index: src/sys/dev/mii/rgephy.c
===================================================================
RCS file: /cvsroot/src/sys/dev/mii/rgephy.c,v
retrieving revision 1.22
retrieving revision 1.23
diff -c -b -p -r1.22 -r1.23
--- src/sys/dev/mii/rgephy.c	17 Nov 2008 03:04:27 -0000	1.22
+++ src/sys/dev/mii/rgephy.c	9 Jan 2009 21:56:35 -0000	1.23
@@ -1,4 +1,4 @@
-/*	$NetBSD: rgephy.c,v 1.22 2008/11/17 03:04:27 dyoung Exp $	*/
+/*	$NetBSD: rgephy.c,v 1.23 2009/01/09 21:56:35 cegger Exp $	*/
 
 /*
  * Copyright (c) 2003
@@ -33,7 +33,7 @@
  */
 
 #include <sys/cdefs.h>
-__KERNEL_RCSID(0, "$NetBSD: rgephy.c,v 1.22 2008/11/17 03:04:27 dyoung Exp $");
+__KERNEL_RCSID(0, "$NetBSD: rgephy.c,v 1.23 2009/01/09 21:56:35 cegger Exp $");
 
 
 /*
@@ -483,8 +483,13 @@
 static void
 rgephy_load_dspcode(struct mii_softc *sc)
 {
+	struct rgephy_softc *rsc;
 	int val;
 
+	rsc = (struct rgephy_softc *)sc;
+	if (rsc->mii_revision >= 2)
+		return;
+
 #if 1
 	PHY_WRITE(sc, 31, 0x0001);
 	PHY_WRITE(sc, 21, 0x1000);
@@ -578,14 +583,22 @@
 rgephy_reset(struct mii_softc *sc)
 {
 	struct rgephy_softc *rsc;
+	uint16_t ssr;
 
 	mii_phy_reset(sc);
 	DELAY(1000);
 
 	rsc = (struct rgephy_softc *)sc;
-	if (rsc->mii_revision < 2)
+	if (rsc->mii_revision < 2) {
 		rgephy_load_dspcode(sc);
-	else {
+	} else if (rsc->mii_revision == 3) {
+		/* RTL8211C(L) */
+		ssr = PHY_READ(sc, RGEPHY_MII_SSR);
+		if ((ssr & RGEPHY_SSR_ALDPS) != 0) {
+			ssr &= ~RGEPHY_SSR_ALDPS;
+			PHY_WRITE(sc, RGEPHY_MII_SSR, ssr);
+		}
+	} else {
 		PHY_WRITE(sc, 0x1F, 0x0001);
 		PHY_WRITE(sc, 0x09, 0x273a);
 		PHY_WRITE(sc, 0x0e, 0x7bfb);
