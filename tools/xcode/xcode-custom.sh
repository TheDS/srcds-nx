buildAction () {
	echo "Building..."
}

cleanAction () {
	echo "Cleaning..."
	cd "${TARGET_BUILD_DIR}"
	buildNumber=$(/usr/libexec/PlistBuddy -c "Print CFBundleVersion" "srcds-cli.bundle/Contents/Info.plist")
	rm -f srcds.sh
	rm -f srcds-nx-$buildNumber.zip
}

echo "Running with ACTION=${ACTION}"

case $ACTION in
	# NOTE: for some reason, it gets set to "" rather than "build" when
	# doing a build.
	"")
		buildAction
		;;

	"clean")
		cleanAction
		;;
esac

exit 0
