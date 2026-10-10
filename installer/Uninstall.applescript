on run
	set scriptPath to POSIX path of (path to resource "uninstall.sh")

	display dialog "This will remove CP1 (the plugin and the standalone app) from this Mac." & return & return & "Your saved presets are kept." buttons {"Cancel", "Uninstall"} default button "Cancel" with title "Uninstall CP1" with icon caution

	-- Close the standalone app if it is open.
	try
		do shell script "/usr/bin/pkill -x CP1"
	end try

	try
		do shell script "/bin/bash " & quoted form of scriptPath with administrator privileges
		display dialog "CP1 has been removed." & return & return & "Please restart your music software." buttons {"OK"} default button "OK" with title "Uninstall CP1"
	on error errMsg number errNum
		if errNum is not -128 then
			display dialog "Something went wrong:" & return & errMsg buttons {"OK"} default button "OK" with title "Uninstall CP1" with icon stop
		end if
	end try
end run
