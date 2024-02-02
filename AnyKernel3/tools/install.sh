#!/sbin/sh

#get slot
get_slot=$(getprop ro.boot.slot_suffix 2>/dev/null);
block=/dev/block/bootdevice/by-name/boot;

# import functions/variables and setup patching - see for reference (DO NOT REMOVE)
. tools/ak3-core.sh

dump_boot # use split_boot to skip ramdisk unpack, e.g. for devices with init_boot ramdisk

patch_cmdline "skip_override" "";

cekdevice=$(getprop ro.product.device 2>/dev/null);
cekproduct=$(getprop ro.build.product 2>/dev/null);
cekvendordevice=$(getprop ro.product.vendor.device 2>/dev/null);
cekvendorproduct=$(getprop ro.vendor.product.device 2>/dev/null);
getmodel=$(getprop ro.product.model 2>/dev/null);
for cekdevicename in $cekdevice $cekproduct $cekvendordevice $cekvendorproduct; do
	cekdevices=$cekdevicename
	break 1;
done;

# read ram
read_ram=$(free | grep Mem |  awk '{print $2}')
if [ $read_ram -lt 4000000 ]; then
ram="4 GB"
elif [ $read_ram -lt 6000000 ]; then
ram="6 GB"
elif [ $read_ram -lt 8000000 ]; then
ram="8 GB"
elif [ $read_ram -lt 12000000 ]; then
ram="12 GB"
elif [ $read_ram -lt 16000000 ]; then
ram="16 GB"
else
ram=$read_ram
fi

# Clear
ui_print " ";
ui_print " ";
ui_print "#";
ui_print "# D8G Kernel"
ui_print "# by diphons" 
ui_print "#";
ui_print " ";
ui_print "• Devices : $cekdevices ";
ui_print "• Model : $getmodel ";
ui_print "• RAM : $ram ";
ui_print " ";
ui_print " ";

keytest() {
  ui_print "• Press a Vol Key"
  (/system/bin/getevent -lc 1 2>&1 | /system/bin/grep VOLUME | /system/bin/grep " DOWN" > /tmp/anykernel/events) || return 1
  return 0
}

chooseport() {
  #note from chainfire @xda-developers: getevent behaves weird when piped, and busybox grep likes that even less than toolbox/toybox grep
  while (true); do
    /system/bin/getevent -lc 1 2>&1 | /system/bin/grep VOLUME | /system/bin/grep " DOWN" > /tmp/anykernel/events
    if (`cat /tmp/anykernel/events 2>/dev/null | /system/bin/grep VOLUME >/dev/null`); then
      break
    fi
  done
  if (`cat /tmp/anykernel/events 2>/dev/null | /system/bin/grep VOLUMEUP >/dev/null`); then
    return 0
  else
    return 1
  fi
}

chooseportold() {
  # Calling it first time detects previous input. Calling it second time will do what we want
  $bin/keycheck
  $bin/keycheck
  SEL=$?
  if [ "$1" == "UP" ]; then
    UP=$SEL
  elif [ "$1" == "DOWN" ]; then
    DOWN=$SEL
  elif [ $SEL -eq $UP ]; then
    return 0
  elif [ $SEL -eq $DOWN ]; then
    return 1
  else
    abort "• Vol key not detected!"
  fi
}

if keytest; then
  FUNCTION=chooseport
else
  FUNCTION=chooseportold
  ui_print "• Press Vol Up Again"
  $FUNCTION "UP"
  ui_print "• Press Vol Down"
  $FUNCTION "DOWN"
fi

# Install Kernel

# Clear
ui_print " ";
ui_print " ";

# Choose DFE
ui_print " "
ui_print "Install DFE ?"
ui_print "   Vol+ = Yes, Vol- = No"
ui_print "   Yes!!... Install DFE"
ui_print "   No!!... Skip Install DFE"
ui_print " "
if $FUNCTION; then
	ui_print "-> Install DFE Selected.."
	install_dfe="• DFE : Install"
	. /tmp/anykernel/tools/fstab.sh;
else
	ui_print "-> Skip Install DFE Selected.."
	install_dfe="• DFE : Skip Install"
fi

# Choose Permissive or Enforcing
ui_print " "
ui_print "Choose Default Selinux to Install.."
ui_print " "
ui_print "Permissive Or Enforcing Kernel?"
ui_print " "
ui_print "   Vol+ = Yes, Vol- = No"
ui_print ""
ui_print "   Yes.. Permissive"
ui_print "   No!!... Enforcing"
ui_print " "

if $FUNCTION; then
	ui_print "-> Permissive Kernel Selected.."
	install_pk="• Selinux : Permissive"
	patch_cmdline androidboot.selinux androidboot.selinux=permissive
else
	ui_print "-> Enforcing Kernel Selected.."
	install_pk="• Selinux : Enforcing"
	patch_cmdline androidboot.selinux androidboot.selinux=enforcing
fi

check_android_version(){
	umount /system || true
	umount /vendor || true
	mount -o rw /dev/block/bootdevice/by-name/system /system
	mount -o rw /dev/block/bootdevice/by-name/vendor /vendor
	if [ -f /system/build.prop ]; then
		patch_build=/system/build.prop
	else
		if [ -f /system/system/build.prop ]; then
			patch_build=/system/system/build.prop
		else
			if [ -f /system_root/system/build.prop ]; then
				patch_build=/system_root/system/build.prop
			else
				if [ -f /system/system_root/system/build.prop ]; then
					patch_build=/system_root/system/build.prop
				else
					patch_build=0
				fi
			fi
		fi
	fi;

	if [ $patch_build = 0 ]; then
		install_av="• Android : Not Detected"
	else
		if ! grep -q 'ro.system.build.version.sdk=35' $patch_build; then
			if ! grep -q 'ro.system.build.version.sdk=34' $patch_build; then
				if ! grep -q 'ro.system.build.version.sdk=33' $patch_build; then
					if ! grep -q 'ro.system.build.version.sdk=32' $patch_build; then
						if ! grep -q 'ro.system.build.version.sdk=31' $patch_build; then
							install_av="• Android : 11"
						else
							install_av="• Android : 12"
						fi
					else
						install_av="• Android : 12.1"
					fi
				else
					install_av="• Android : 13"
				fi
			else
				install_av="• Android : 14"
			fi
		else
			install_av="• Android : 15"
		fi
	fi
	umount /system || true
	umount /vendor || true
}

header_install(){
	check_android_version
	ui_print " "
	ui_print " "
	ui_print "Flashing Kernel :"
	ui_print "------------------------------------"
	ui_print "• Device : $cekdevicename"
	ui_print "• Model : $getmodel ";
	ui_print "• RAM : $ram ";
	ui_print "$install_av"
	ui_print "$install_ocd"
	ui_print "$install_dhz"
	ui_print "$install_pk"
	ui_print "$install_dfe"
}
header_abort(){
	# reset for boot patching
	reset_ak;
	ui_print " "
	ui_print " "
	ui_print "Aborting"
	ui_print "Image not ready to be flash"
	ui_print " "
	break 1;
}
if [[ $cekdevices == "beryllium" ]] || [[ $cekdevices == "PocoF1" ]] || [[ $cekdevices == "PocophoneF1" ]]; then
	dir_gpu=0
	vhz=60
	dt_dir=$home/kernel/sdm845
	if [ -f $dt_dir ]; then
		cd $home/kernel
		mv -f sdm845 sdm845.gz
		if [ -d sdm845 ]; then
			rm -fr sdm845;
		fi;
		$bin/busybox tar -xf sdm845.gz sdm845
		rm -f sdm845.gz
		cd $home
	fi;
	# display Select
	select_ocd(){
		# Choose dts
		ui_print " "
		ui_print "Choose your favorite display hz?"
		ui_print " "
		ui_print "Jangan dipaksa, gunakan semampu device kalian"
		ui_print " "
		ui_print "   Vol+ = Yes, Vol- = No"
		ui_print ""
		ui_print "   Yes!!... Install FPS 60hz"
		ui_print "   No!!... Choose again"
		ui_print " "
		if $FUNCTION; then
			ui_print "-> Display 60hz Selected.."
			install_dhz="• Display : 60hz";
			#if [[ -f $compressed_image ]; then
			# Concatenate all of the dtbs to the kernel
			vhz=60
			#fi
		else
			ui_print "   Vol+ = Yes, Vol- = No"
			ui_print "   Yes!!... Install FPS 61hz"
			ui_print "   No!!... Choose again"
			ui_print " "
			if $FUNCTION; then
				# 61hz
				ui_print "-> Display 61hz Selected.."
				install_dhz="• Display : 61hz";
				#if [[ -f $compressed_image ]; then
				# Concatenate all of the dtbs to the kernel
				vhz=61
				#fi
			else
				ui_print "   Vol+ = Yes, Vol- = No"
				ui_print "   Yes!!... Install FPS 65hz"
				ui_print "   No!!... Choose again"
				ui_print " "
				if $FUNCTION; then
					# 65hz
					ui_print "-> Display 65hz Selected.."
					install_dhz="• Display : 65hz";
					#if [[ -f $compressed_image ]; then
					# Concatenate all of the dtbs to the kernel
					vhz=65
					#fi
				else
					ui_print "   Vol+ = Yes, Vol- = No"
					ui_print "   Yes!!... Install FPS 66hz"
					ui_print "   No!!... Choose again"
					ui_print " "
					if $FUNCTION; then
						# 66hz
						ui_print "-> Display 66hz Selected.."
						install_dhz="• Display : 66hz";
						#if [[ -f $compressed_image ]; then
						# Concatenate all of the dtbs to the kernel
						vhz=66
						#fi
					else
						ui_print "   Vol+ = Yes, Vol- = No"
						ui_print "   Yes!!... Install FPS 67hz"
						ui_print "   No!!... Choose again"
						ui_print " "
						if $FUNCTION; then
							# 67hz
							ui_print "-> Display 67hz Selected.."
							install_dhz="• Display : 67hz";
							#if [[ -f $compressed_image ]; then
								# Concatenate all of the dtbs to the kernel
								vhz=67
							#fi
						else
							ui_print "   Vol+ = Yes, Vol- = No"
							ui_print "   Yes!!... Install FPS 68hz"
							ui_print "   No!!... Choose again"
							ui_print " "
							if $FUNCTION; then
								# 68hz
								ui_print "-> Display 68hz Selected.."
								install_dhz="• Display : 68hz";
								#	if [[ -f $compressed_image ]; then
								# Concatenate all of the dtbs to the kernel
								vhz=68
								#fi
							else
								ui_print "   Vol+ = Yes, Vol- = No"
								ui_print "   Yes!!... Install FPS 69hz"
								ui_print "   No!!... Choose again"
								ui_print " "
								if $FUNCTION; then
									# 69hz
									ui_print "-> Display 69hz Selected.."
									install_dhz="• Display : 69hz";
									#if [[ -f $compressed_image ]; then
										# Concatenate all of the dtbs to the kernel
									vhz=69
									#fi
								else
									ui_print "   Vol+ = Yes, Vol- = No"
									ui_print "   Yes!!... Install FPS 70hz"
									ui_print "   No!!... Choose again"
									ui_print " "
									if $FUNCTION; then
										# 70hz
										ui_print "-> Display 70hz Selected.."
										install_dhz="• Display : 70hz";
										#if [[ -f $compressed_image ]; then
											# Concatenate all of the dtbs to the kernel
										vhz=70
										#fi
									else
										ui_print "   Vol+ = Yes, Vol- = No"
										ui_print "   Yes!!... Install FPS 71hz"
										if [ -f $dt_dir/$dir_gpu/75/beryllium-mp-v2.1.dtb ]; then
											ui_print "   No!!... Install Stock FPS - 75hz"
										elif [ -f $dt_dir/$dir_gpu/76/beryllium-mp-v2.1.dtb ]; then
											ui_print "   No!!... Install Stock FPS - 76hz"
										elif [ -f $dt_dir/$dir_gpu/77/beryllium-mp-v2.1.dtb ]; then
											ui_print "   No!!... Install Stock FPS - 77hz"
										elif [ -f $dt_dir/$dir_gpu/78/beryllium-mp-v2.1.dtb ]; then
											ui_print "   No!!... Install Stock FPS - 78hz"
										elif [ -f $dt_dir/$dir_gpu/80/beryllium-mp-v2.1.dtb ]; then
											ui_print "   No!!... Install Stock FPS - 80hz"
										elif [ -f $dt_dir/$dir_gpu/81/beryllium-mp-v2.1.dtb ]; then
											ui_print "   No!!... Install Stock FPS - 81hz"
										elif [ -f $dt_dir/$dir_gpu/90/beryllium-mp-v2.1.dtb ]; then
											ui_print "   No!!... Install Stock FPS - 90hz"
										else
											ui_print "   No!!... Install Stock FPS - 60hz"
										fi;
										ui_print " "
										if $FUNCTION; then
											# 71hz
											ui_print "-> Display 71hz Selected.."
											install_dhz="• Display : 71hz";
											#if [[ -f $compressed_image ]; then
												# Concatenate all of the dtbs to the kernel
												vhz=71
											#fi
										else
											if [ -f $dt_dir/$dir_gpu/75/beryllium-mp-v2.1.dtb ]; then
												set75
											elif [ -f $dt_dir/$dir_gpu/76/beryllium-mp-v2.1.dtb ]; then
												set76
											elif [ -f $dt_dir/$dir_gpu/77/beryllium-mp-v2.1.dtb ]; then
												set77
											elif [ -f $dt_dir/$dir_gpu/78/beryllium-mp-v2.1.dtb ]; then
												set78
											elif [ -f $dt_dir/$dir_gpu/80/beryllium-mp-v2.1.dtb ]; then
												set80
											elif [ -f $dt_dir/$dir_gpu/81/beryllium-mp-v2.1.dtb ]; then
												set81
											else
												set90
											fi
										fi
									fi
								fi
							fi
						fi
					fi
				fi
			fi
		fi
		cat $dt_dir/Image.gz $dt_dir/$dir_gpu/$vhz/*.dtb > $home/Image.gz-dtb;
	}

	gpu_select1(){
		if [ -d $dt_dir/1 ]; then
			ui_print " "
			ui_print "Choose GPU to install.."
			ui_print " "
			ui_print "Select GPU OC"
			ui_print " "
			ui_print "   Vol+ = Yes, Vol- = No"
			ui_print ""
			ui_print "   Yes.. OC 800 MHz"
			ui_print "   No!!... Choose again"
			ui_print " "
			if $FUNCTION; then
				ui_print "-> OC GPU 800 MHz Selected.."
				install_ocd="• GPU : OC 800 MHz"
				dir_gpu=1;
				select_ocd;
			else
				gpu_select2
			fi;
		else
			gpu_select2
		fi
	}

	gpu_select2(){
		if [ -d $dt_dir/2 ]; then
			ui_print " "
			ui_print "Choose GPU to install.."
			ui_print " "
			ui_print "Select GPU OC"
			ui_print " "
			ui_print "   Vol+ = Yes, Vol- = No"
			ui_print ""
			ui_print "   Yes.. OC 820 MHz"
			ui_print "   No!!... Choose again"
			ui_print " "
			if $FUNCTION; then
				ui_print "-> OC GPU 820 MHz Selected.."
				install_ocd="• GPU : OC 820 MHz"
				dir_gpu=2;
				select_ocd;
			else
				gpu_select3
			fi;
		else
			gpu_select3
		fi
	}

	gpu_select3(){
		if [ -d $dt_dir/3 ]; then
			ui_print " "
			ui_print "Choose GPU to install.."
			ui_print " "
			ui_print "Select GPU OC"
			ui_print " "
			ui_print "   Vol+ = Yes, Vol- = No"
			ui_print ""
			ui_print "   Yes.. OC 835 MHz"
			ui_print "   No!!... Choose again"
			ui_print " "
			if $FUNCTION; then
				ui_print "-> OC GPU 835 MHz Selected.."
				install_ocd="• GPU : OC 835 MHz"
				dir_gpu=3;
				select_ocd;
			else
				gpu_select4
			fi;
		else
			gpu_select4
		fi
	}

	gpu_select4(){
		if [ -d $dt_dir/4 ]; then
			ui_print " "
			ui_print "Choose GPU to install.."
			ui_print " "
			ui_print "Select GPU OC"
			ui_print " "
			ui_print "   Vol+ = Yes, Vol- = No"
			ui_print ""
			ui_print "   Yes.. OC 840 MHz"
			ui_print "   No!!... Choose again"
			ui_print " "
			if $FUNCTION; then
				ui_print "-> OC GPU 840 MHz Selected.."
				install_ocd="• GPU : OC 840 MHz"
				dir_gpu=4;
				select_ocd;
			else
				gpu_select5;
			fi;
		else
			gpu_select5;
		fi
	}

	gpu_select5(){
		if [ -d $dt_dir/5 ]; then
			ui_print " "
			ui_print "Choose GPU to install.."
			ui_print " "
			ui_print "Select GPU OC"
			ui_print " "
			ui_print "   Vol+ = Yes, Vol- = No"
			ui_print ""
			ui_print "   Yes.. OC 845 MHz"
			ui_print "   No!!... Choose again"
			ui_print " "
			if $FUNCTION; then
				ui_print "-> OC GPU 845 MHz Selected.."
				install_ocd="• GPU : OC 845 MHz"
				dir_gpu=5;
				select_ocd;
			else
				gpu_select6
			fi;
		else
			gpu_select6
		fi
	}

	gpu_select6(){
		if [ -d $dt_dir/6 ]; then
			ui_print " "
			ui_print "Choose GPU to install.."
			ui_print " "
			ui_print "Select GPU OC"
			ui_print " "
			ui_print "   Vol+ = Yes, Vol- = No"
			ui_print ""
			ui_print "   Yes.. OC 855 MHz"
			ui_print "   No!!... Choose again"
			ui_print " "
			if $FUNCTION; then
				ui_print "-> OC GPU 855 MHz Selected.."
				install_ocd="• GPU : OC 855 MHz"
				dir_gpu=6;
				select_ocd;
			else
				gpu_select
			fi;
		else
			gpu_select
		fi
	}

	stock_mode(){
		ui_print "-> Stock Selected.."
		install_ocd="• GPU : Stock"
		install_dhz="• Display : 60hz";
		dir_gpu=0;
		vhz=60;
		cat $dt_dir/Image.gz $dt_dir/$dir_gpu/$vhz/*.dtb > $home/Image.gz-dtb;
	}

	gpu_select(){
		if [[ -d $dt_dir/1 ]] || [[ -d $dt_dir/2 ]] || [[ -d $dt_dir/3 ]] || [[ -d $dt_dir/4 ]] || [[ -d $dt_dir/5 ]] || [[ -d $dt_dir/6 ]]; then
			ui_print " "
			ui_print "Choose OC - Non OC.."
			ui_print " "
			ui_print "Select OC or Stock?"
			ui_print " "
			ui_print "   Vol+ = Yes, Vol- = No"
			ui_print ""
			ui_print "   Yes.. OC Mode"
			ui_print "   No!!... Stock Mode"
			ui_print " "
			if $FUNCTION; then
				ui_print "-> With OC Selected.."
				# Choose Permissive or Enforcing
				ui_print " "
				ui_print "Choose GPU OC - Non OC.."
				ui_print " "
				ui_print "Select GPU OC or Stock?"
				ui_print " "
				ui_print "   Vol+ = Yes, Vol- = No"
				ui_print ""
				ui_print "   Yes.. OC GPU"
				ui_print "   No!!... Stock GPU"
				ui_print " "
				if $FUNCTION; then
					gpu_select1;
				else
					ui_print "-> Stock GPU Selected.."
					install_ocd="• GPU : Stock"
					dir_gpu=0;
					select_ocd;
				fi;
			else
				stock_mode
			fi
		else
			ui_print " "
			ui_print "Choose OC - Non OC.."
			ui_print " "
			ui_print "Select Display OC or Stock?"
			ui_print " "
			ui_print "   Vol+ = Yes, Vol- = No"
			ui_print ""
			ui_print "   Yes.. Chose display framerate"
			ui_print "   No!!... Stock display"
			ui_print " "
			if $FUNCTION; then
				install_ocd="• GPU : Stock"
				dir_gpu=0;
				select_ocd;
			else
				stock_mode
			fi
		fi
	}

	if [ -f $dt_dir/$dir_gpu/$vhz/beryllium-mp-v2.1.dtb ]; then
		gpu_select
	else
		if [ -f  $dt_dir/Image.gz-dtb ]; then
			cp $dt_dir/Image.gz-dtb $home/Image.gz-dtb
		fi
		if [ -f  $home/kernel/Image.gz-dtb ]; then
			cp $home/kernel/Image.gz-dtb $home/Image.gz-dtb
		fi
	fi
	# Check image before flashing
	if [ -f $home/Image.gz-dtb ]; then
		header_install
		write_boot # use flash_boot to skip ramdisk repack, e.g. for devices with init_boot ramdisk
		## end boot install
	else
		header_abort;
	fi
else
	cekdevices=$(echo $cekdevicename | sed 's/in//g')
	dt_dir=$home/kernel/$cekdevices
	if [ -f $dt_dir ]; then
		cd $home/kernel
		mv -f $cekdevices $cekdevices.gz
		if [ -d $cekdevices ]; then
			rm -fr $cekdevices;
		fi;
		$bin/busybox tar -xf $cekdevices.gz $cekdevices
		rm -f $cekdevices.gz
		cd $home
	fi;
	if [ -d $dt_dir ]; then
		if [ -f $dt_dir/Image.gz ]; then
			cp $dt_dir/Image.gz $home/Image.gz
		fi
		dtbo_aosp=$dt_dir/dtbo_aosp.img
		dtbo_aosp_90=$dt_dir/dtbo_aosp_90.img
		select_ocd(){
			if [ -f $dtbo_aosp_90 ]; then
				ui_print " "
				ui_print "Choose FPS to Install.."
				ui_print " "
				ui_print "Add 90 FPS on kernel ?"
				ui_print " "
				ui_print "   Vol+ = Yes, Vol- = No"
				ui_print ""
				ui_print "   Yes.. Add 90 FPS"
				ui_print "   No!!... Use Stock FPS"
				ui_print " "
				if $FUNCTION; then
					ui_print "-> Add 90 FPS selected.."
					install_dhz="• Display : Add 90hz";
					nine_set=1
				else
					ui_print "-> Use Stock FPS selected.."
					install_dhz="• Display : Stock";
					nine_set=0
				fi
			else
				install_dhz="• Display : Stock";
				nine_set=0
			fi
		}
		if [ -f $dtbo_aosp ]; then
			ui_print " "
			ui_print "Choose Vendor Rom installed.."
			ui_print " "
			ui_print "MIUI or AOSP ?"
			ui_print " "
			ui_print "   Vol+ = Yes, Vol- = No"
			ui_print ""
			ui_print "   Yes.. MIUI | OxygenOS"
			ui_print "   No!!... AOSP"
			ui_print " "
			if $FUNCTION; then
				ui_print "-> MIUI selected.."
				install_ocd="• ROM : MIUI"
				vendor_mode=0
			else
				ui_print "-> AOSP selected.."
				install_ocd="• ROM : AOSP"
				vendor_mode=1
			fi
			select_ocd;
		else
			install_ocd="• ROM : MIUI"
			install_dhz="• Display : Stock";
			vendor_mode=0
			nine_set=0
		fi;

		if [ $vendor_mode = 1 ]; then
			if [ $nine_set = 1 ]; then
				cp $dtbo_aosp_90 $home/dtbo.img
			else
				cp $dtbo_aosp $home/dtbo.img
			fi
		else
			if [ $nine_set = 1 ]; then
				cp $dt_dir/dtbo_90.img $home/dtbo.img
			else
				cp $dt_dir/dtbo.img $home/dtbo.img
			fi
		fi
		if [ -f $home/kernel/kona ]; then
			cd $home/kernel
			mv -f kona kona.gz
			if [ -d kona ]; then
				rm -fr kona;
			fi;
			$bin/busybox tar -xf kona.gz kona
			rm -f kona.gz
			dtb_dir=$home/kernel/kona
			cd $home
		else
			dtb_dir=$dt_dir
		fi;
		# dtb
		dtb_image=$dtb_dir/dtb
		dtb_image_oc=$dtb_dir/dtb_oc
		dtb_image_v=$dtb_dir/dtb_v
		dtb_image_voc=$dtb_dir/dtb_voc
		if [ -f $dtb_image_oc ]; then
			ui_print " "
			ui_print "Choose GPU to install.."
			ui_print " "
			ui_print "Over Clock GPU ?"
			ui_print " "
			ui_print "   Vol+ = Yes, Vol- = No"
			ui_print ""
			ui_print "   Yes.. Over Clock GPU"
			ui_print "   No!!... Stock with Under Clock GPU"
			ui_print " "
			if $FUNCTION; then
				if [ -f $dtb_image_voc ]; then
					ui_print " "
					ui_print "Choose GPU to install.."
					ui_print " "
					ui_print "Undervolt GPU ?"
					ui_print " "
					ui_print "   Vol+ = Yes, Vol- = No"
					ui_print ""
					ui_print "   Yes.. Undervolt GPU"
					ui_print "   No!!... Stock volt GPU"
					ui_print " "
					if $FUNCTION; then
						ui_print "-> Include DTB with UV OC GPU selected.."
						install_ocd="• GPU :  OC - UV"
						cp $dtb_image_voc $home/dtb
					else
						ui_print "-> Include DTB with OC GPU selected.."
						install_ocd="• GPU : OC"
						cp $dtb_image_oc $home/dtb
					fi
				else
					ui_print "-> Include DTB with OC GPU selected.."
					install_ocd="• GPU : OC"
					cp $dtb_image_oc $home/dtb
				fi
			else
				if [ -f $dtb_image_v ]; then
					ui_print " "
					ui_print "Choose GPU to install.."
					ui_print " "
					ui_print "Undervolt GPU ?"
					ui_print " "
					ui_print "   Vol+ = Yes, Vol- = No"
					ui_print ""
					ui_print "   Yes.. Undervolt GPU"
					ui_print "   No!!... Stock volt GPU"
					ui_print " "
					if $FUNCTION; then
						ui_print "-> Include DTB with UV Stock GPU selected.."
						install_ocd="• GPU : Stock - UV"
						cp $dtb_image_v $home/dtb
					else
						ui_print "-> Include DTB with Stock GPU selected.."
						install_ocd="• GPU : Stock"
						cp $dtb_image $home/dtb
					fi
				else
					ui_print "-> Include DTB with Stock GPU selected.."
					install_ocd="• GPU : Stock"
					cp $dtb_image $home/dtb
				fi
			fi
		else
			if [ -f $dtb_image_v ]; then
				ui_print " "
				ui_print "Choose GPU to install.."
				ui_print " "
				ui_print "Undervolt GPU ?"
				ui_print " "
				ui_print "   Vol+ = Yes, Vol- = No"
				ui_print ""
				ui_print "   Yes.. Undervolt GPU"
				ui_print "   No!!... Stock volt GPU"
				ui_print " "
				if $FUNCTION; then
					ui_print "-> Include DTB with UV Stock GPU selected.."
					install_ocd="• GPU : Stock - UV"
					cp $dtb_image_v $home/dtb
				else
					ui_print "-> Include DTB with Stock GPU selected.."
					install_ocd="• GPU : Stock"
					cp $dtb_image $home/dtb
				fi
			else
				ui_print "-> Include DTB with Stock GPU selected.."
				install_ocd="• GPU : Stock"
				cp $dtb_image $home/dtb
			fi
		fi

		vendor_boot_patch(){
			#cleanup
			rm -f $home/dtbo.img $home/image.gz
			# vendor_boot shell variables
			block=/dev/block/bootdevice/by-name/vendor_boot;
			is_slot_device=auto;
			ramdisk_compression=auto;
			patch_vbmeta_flag=auto;

			# reset for vendor_boot patching
			reset_ak;

			# vendor_boot install
			dump_boot; # use split_boot to skip ramdisk unpack, e.g. for dtb on devices with hdr v4 but no vendor_kernel_boot

			write_boot; # use flash_boot to skip ramdisk repack, e.g. for dtb on devices with hdr v4 but no vendor_kernel_boot
			## end vendor_boot install
		}

		# Check image before flashing
		if [ -f $home/Image.gz ]; then
			header_install
			write_boot # use flash_boot to skip ramdisk repack, e.g. for devices with init_boot ramdisk
			## end boot install
		else
			header_abort;
		fi

		# Check vendor dtb before flashing
		if [ -f $home/dtb ]; then
			ui_print " "
			ui_print " "
			ui_print "Vendor Boot"
			ui_print "------------------------------------"
			if [ $cekdevices = "apollo" ]; then
				ui_print "No vendor boot. Skip"
			else
				vendor_boot_patch
			fi;
		fi;
	else
		reset_ak;
		ui_print " "
		ui_print " "
		ui_print "Aborting"
		ui_print "Unspesified device"
		ui_print " "
		break 1;
	fi
fi;
