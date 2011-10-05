#!/bin/sh

CLINE=$(getopt -o h \
	--long help,builddir:,srcdir:,hash:,husk: -n "${0}" -- "${@}")
eval set -- "${CLINE}"

usage()
{
	cat <<EOF 
$(basename ${0}) [OPTION] TEST_FILE

--builddir=DIR  specify where tools can be found
--srcdir=DIR    specify where the source tree resides
--hash=PROG     use hasher PROG instead of md5sum
--husk=PROG     use husk around tool, e.g. 'valgrind -v'

-h, --help      print a short help screen
EOF
}

while true; do
	case "${1}" in
	"-h"|"--help")
		usage
		exit 0
		;;
	"--builddir")
		builddir="${2}"
		shift 2
		;;
	"--srcdir")
		srcdir="${2}"
		shift 2
		;;
	"--hash")
		hash="${2}"
		shift 2
		;;
	"--husk")
		HUSK="${2}"
		shift 2
		;;
	--)
		shift
		break
		;;
	*)
		echo "could not parse options" >&2
		exit 1
		;;
	esac
done

## setup
fail=0
tool_stdout=$(mktemp)
tool_stderr=$(mktemp)
pwd=$(pwd)

## source the check
. "${1}" || fail=1

rm_if_not_src()
{
	file="${1}"
	srcd="${2:-${srcdir}}"
	dirf=$(dirname "${file}")

	if test "${dirf}" = "." -a "$(pwd)" = "${srcd}" -a -r "${file}"; then
		## treat as precious source file
		:
	elif test "${dirf}" = "${srcd}"; then
		## treat as precious source file
		:
	else
		rm -vf -- "${file}"
	fi
}

myexit()
{
	rm_if_not_src "${stdin}" "${srcdir}"
	rm_if_not_src "${stdout}" "${srcdir}"
	rm_if_not_src "${stderr}" "${srcdir}"
	rm -f -- "${tool_stdout}" "${tool_stderr}"
	cd "${pwd}"
	exit ${1:-1}
}

## check if everything's set
if test -z "${TOOL}"; then
	echo "variable \${TOOL} not set" >&2
	myexit 1
fi

## set finals
if test -z "${srcdir}"; then
	srcdir=$(dirname "${0}")
fi
if test -x "${builddir}/${TOOL}"; then
	TOOL=$(readlink -e "${builddir}/${TOOL}")
fi

srcdir=$(readlink -e "${srcdir}")
cd "${srcdir}"
eval "${HUSK}" "${TOOL}" "${CMDLINE}" \
	< "${stdin:-/dev/null}" \
	> "${tool_stdout}" 2> "${tool_stderr}" || myexit 1

if test -r "${stdout}"; then
	diff -u "${stdout}" "${tool_stdout}" || fail=1
elif test -s "${tool_stdout}"; then
	echo
	echo "test stdout was:"
	cat "${tool_stdout}" >&2
fi
if test -r "${stderr}"; then
	diff -u "${stderr}" "${tool_stderr}" || fail=1
elif test -s "${tool_stderr}"; then
	echo
	echo "test stderr was:"
	cat "${tool_stderr}" >&2
fi

myexit ${fail}

## dt-test.sh ends here
