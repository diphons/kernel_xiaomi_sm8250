#!/sbin/sh

# Import Fstab
#. /tmp/anykernel/tools/fstab.sh;

patch_cmdline "skip_override" "";

# Clear
ui_print "";
ui_print "";

keytest() {
  ui_print "   Press a Vol Key..."
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
    abort "   Vol key not detected!"
  fi
}

if keytest; then
  FUNCTION=chooseport
else
  FUNCTION=chooseportold
  ui_print "   Press Vol Up Again..."
  $FUNCTION "UP"
  ui_print "   Press Vol Down..."
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
	# Choose DFE
	ui_print " "
	ui_print "Install DFE with system_ext ?"
	ui_print "   Vol+ = Yes, Vol- = No"
	ui_print "   Yes!!... Install with system_ext"
	ui_print "   No!!... Install no system_ext"
	ui_print " "
	if $FUNCTION; then
		ui_print "-> Install DFE with system_ext Selected.."
		cp $home/kernel/dfe/fstab.qcom $home/ramdisk/fstab.qcom
	else
		ui_print "-> Install DFE no system_ext Selected.."
		cp $home/kernel/dfe/fstab_nse.qcom $home/ramdisk/fstab.qcom
	fi
	. /tmp/anykernel/tools/fstab.sh;
else
	ui_print "-> Skip Install DFE Selected.."
fi
