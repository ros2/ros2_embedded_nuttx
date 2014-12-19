############################################################
# PREREQUIT
############################################################
You must be in a ClearCase view

############################################################
# COMPILATION
############################################################

# MFL / log compilation
cd /middleware/core/mfl/log/project/
pwd
make -f Makefile_component E=HOSTLINUX log_clean
make -f Makefile_component E=HOSTLINUX
date
ls -ltr /vobs/do_vob1/do_store/log/HOSTLINUX/

# ServiceFramework / dds compilation
cd /middleware/core/servicefw/dds/project/
pwd
make -f Makefile_component E=HOSTLINUX dds_clean
make -f Makefile_component E=HOSTLINUX
date
ls -ltr /vobs/do_vob1/do_store/dds/HOSTLINUX/

# US 403 main test compilation upon technicolor DDS
cd /middleware/core/servicefw/dds/project/test/systest/bin
pwd
make -f techdds_Makefile_main clean
make -f techdds_Makefile_main
date
ls -ltr 

############################################################
# US 403 TEST CASE EXECUTION
############################################################

Test Case 1:
------------
cd /middleware/core/servicefw/dds/project/test/systest/script
./techdds_US403_TC1.sh
Then check output log messages in /var/log/messages (from syslog)

Test Case 2:
------------
cd /middleware/core/servicefw/dds/project/test/systest/script
./techdds_US403_TC2.sh
Then check output log messages in /var/log/messages (from syslog)

