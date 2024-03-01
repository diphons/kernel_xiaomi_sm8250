#!/sbin/sh

dfe_main(){
find $fstab_dir -name "fstab.*" -exec sed -i 's/forceencrypt/encryptable/g' {} +
find $fstab_dir -name "fstab.*" -exec sed -i 's/forcefdeorfbe/encryptable/g' {} +
find $fstab_dir -name "fstab.*" -exec sed -i 's/fileencryption/encryptable/g' {} +
find $fstab_dir -name "fstab.*" -exec sed -i 's/fileencryption=ice//g' {} +
}

ui_print " "; ui_print "Set DFE mount points...";
umount /vendor || true
if [ -d /dev/block/mapper ]; then
ui_print "mounting vendor /dev/block/mapper/vendor$get_slot...";
mount -o ro /dev/block/mapper/vendor$get_slot /vendor
mount -o remount,rw /vendor
else
ui_print "mounting vendor /dev/block/bootdevice/by-name/vendor...";
mount -o rw /dev/block/bootdevice/by-name/vendor /vendor
fi

ui_print " ";
if [ -d /vendor/bin ]; then
crwv=/vendor/bin/cek_rw_vendor
ui_print " - check writable...";
echo "test" > $crwv
cek_vdr=$(cat $crwv)
if [ $cek_vdr = "test" ]; then
ui_print " - applying dfe...";
rm -f $crwv
fstab_dir=/vendor/etc
dfe_main
ui_print " - set dfe finished...";
umount /vendor || true
if [ -d $ramdisk ]; then
ui_print "ramdisk: check and apply dfe...";
fstab_dir=$ramdisk
dfe_main
ui_print "ramdisk: set dfe finished...";
fi
else
ui_print " - vendor is not writable...";
ui_print " - Aborting dfe...";
fi
else
ui_print " - failed to mount vendor...";
fi;
ui_print " ";
