===================================================================
RCS file: /home/ncvs/src/usr.sbin/sysinstall/disks.c,v
retrieving revision 1.164.2.2.2.1
retrieving revision 1.164.2.3.2.1
diff -c -b -p -r1.164.2.2.2.1 -r1.164.2.3.2.1
*** src/usr.sbin/sysinstall/disks.c 14 Jun 2010 02:09:06 -0000  1.164.2.2.2.1
--- src/usr.sbin/sysinstall/disks.c 21 Dec 2010 17:09:25 -0000  1.164.2.3.2.1
*************** diskPartition(Device *dev)
*** 457,467 ****
      snprintf(tmp, 20, "%jd", (intmax_t)chunk_info[current_chunk]->size);
      val = msgGetInput(tmp, "Please specify the size for new FreeBSD slice in blocks\n"
            "or append a trailing `M' for megabytes (e.g. 20M).");
!     if (val && (size = strtoimax(val, &cp, 0)) > 0) {
          if (*cp && toupper(*cp) == 'M')
!       size *= ONE_MEG;
          else if (*cp && toupper(*cp) == 'G')
!       size *= ONE_GIG;
          sprintf(tmp, "%d", SUBTYPE_FREEBSD);
          val = msgGetInput(tmp, "Enter type of partition to create:\n\n"
        "Pressing Enter will choose the default, a native FreeBSD\n"
--- 458,477 ----
      snprintf(tmp, 20, "%jd", (intmax_t)chunk_info[current_chunk]->size);
      val = msgGetInput(tmp, "Please specify the size for new FreeBSD slice in blocks\n"
            "or append a trailing `M' for megabytes (e.g. 20M).");
!     if (val && (dsize = strtold(val, &cp)) > 0 && dsize < UINT32_MAX) {
          if (*cp && toupper(*cp) == 'M')
!       size = (daddr_t) (dsize * ONE_MEG);
          else if (*cp && toupper(*cp) == 'G')
!       size = (daddr_t) (dsize * ONE_GIG);
!         else
!       size = (daddr_t) dsize;
!
!         if (size < ONE_MEG) {
!       msgConfirm("The minimum slice size is 1MB");
!       break;
!         }
!
!
          sprintf(tmp, "%d", SUBTYPE_FREEBSD);
          val = msgGetInput(tmp, "Enter type of partition to create:\n\n"
        "Pressing Enter will choose the default, a native FreeBSD\n"

