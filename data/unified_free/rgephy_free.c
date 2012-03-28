Index: src/sys/dev/mii/rgephy.c
===================================================================
RCS file: /home/ncvs/src/sys/dev/mii/rgephy.c,v
retrieving revision 1.19
retrieving revision 1.20
diff -c -b -p -r1.19 -r1.20
--- src/sys/dev/mii/rgephy.c	5 Mar 2008 01:15:10 -0000	1.19
+++ src/sys/dev/mii/rgephy.c	2 Jul 2008 08:10:18 -0000	1.20
@@ -31,10 +31,10 @@
  */
 
 #include <sys/cdefs.h>
-__FBSDID("$FreeBSD: src/sys/dev/mii/rgephy.c,v 1.19 2008/03/05 01:15:10 yongari Exp $");
+__FBSDID("$FreeBSD: src/sys/dev/mii/rgephy.c,v 1.20 2008/07/02 08:10:18 yongari Exp $");
 
 /*
- * Driver for the RealTek 8169S/8110S/8211B internal 10/100/1000 PHY.
+ * Driver for the RealTek 8169S/8110S/8211B/8211C internal 10/100/1000 PHY.
  */
 
 #include <sys/param.h>
@@ -531,6 +531,18 @@
 static void
 rgephy_reset(struct mii_softc *sc)
 {
+	struct rgephy_softc *rsc;
+	uint16_t ssr;
+
+	rsc = (struct rgephy_softc *)sc;
+	if (rsc->mii_revision == 3) {
+		/* RTL8211C(L) */
+		ssr = PHY_READ(sc, RGEPHY_MII_SSR);
+		if ((ssr & RGEPHY_SSR_ALDPS) != 0) {
+			ssr &= ~RGEPHY_SSR_ALDPS;
+			PHY_WRITE(sc, RGEPHY_MII_SSR, ssr);
+		}
+	}
 
 	mii_phy_reset(sc);
 	DELAY(1000);
