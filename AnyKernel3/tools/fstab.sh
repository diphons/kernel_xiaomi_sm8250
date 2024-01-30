#!/sbin/sh

ui_print " "; ui_print "Set DFE mount points...";
umount /vendor || true
if [ -f /dev/block/mapper/vendor_a ]; then
ui_print "mounting vendor /dev/block/mapper/vendor_a...";
mount -o rw /dev/block/mapper/vendor_a /vendor
elif [ -f /dev/block/mapper/vendor_b ]; then
ui_print "mounting vendor /dev/block/mapper/vendor_b...";
mount -o rw /dev/block/mapper/vendor_b /vendor
elif [ -f /dev/block/mapper/vendor ]; then
ui_print "mounting vendor /dev/block/mapper/vendor...";
mount -o rw /dev/block/mapper/vendor /vendor
else
ui_print "mounting vendor /dev/block/bootdevice/by-name/vendor...";
mount -o rw /dev/block/bootdevice/by-name/vendor /vendor
fi;

if [ -d /vendor/bin ]; then
ui_print "check and apply dfe...";
find /vendor/etc -name "fstab.*" -exec sed -i 's/forceencrypt/encryptable/g' {} +
find /vendor/etc -name "fstab.*" -exec sed -i 's/forcefdeorfbe/encryptable/g' {} +
find /vendor/etc -name "fstab.*" -exec sed -i 's/fileencryption/encryptable/g' {} +
ui_print "set dfe finished...";
umount /vendor || true
else
ui_print "failed to mount vendor...";
fi;
ui_print " ";
