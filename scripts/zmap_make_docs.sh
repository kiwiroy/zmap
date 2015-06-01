#!/bin/bash


SCRIPT_NAME=$(basename $0)
INITIAL_DIR=$(pwd)
 SCRIPT_DIR=$(dirname $0)
if ! echo $SCRIPT_DIR | egrep -q "(^)/" ; then
    BASE_DIR=$INITIAL_DIR/$SCRIPT_DIR
else
    BASE_DIR=$SCRIPT_DIR
fi

# required by build_config.sh
set -o history

. $BASE_DIR/zmap_functions.sh || { echo "Failed to load zmap_functions.sh"; exit 1; }

. $BASE_DIR/build_config.sh   || { echo "Failed to load build_config.sh";   exit 1; }

zmap_message_out "Start of script."

zmap_message_out "Running in $INITIAL_DIR on $(hostname) under $(uname)"

zmap_message_out "cmd line options = '$*'"

# including VARIABLE=VALUE settings from command line
if [ $# -gt 0 ]; then
    eval "$*"
fi

zmap_cd $BASE_DIR

zmap_goto_cvs_module_root 

ZMAP_CHECKOUT_ROOT=$(pwd)

zmap_cd src

if [ "x$ZMAP_MASTER_BUILD_DOXYGEN_DOCS" == "x$ZMAP_TRUE" ]; then
    zmap_message_out "Running make docs ..."

    # need to find a Makefile to run make against
    MAKEFILE_LOC=$(find . -name Makefile -type f | \
	perl -wlne 'my $c = $_ =~ tr!/!/!; print "$c $_"' | sort -u | \
	perl -wlane 'print $F[1]' | head -1)
    SAVE_DIR=$(pwd)
    zmap_cd $(dirname $MAKEFILE_LOC)

    make docs || zmap_message_exit "Failed making documentation."

    zmap_cd $SAVE_DIR
fi

if [ "x$ENSCRIPT_EXE" != "x" ]; then

    zmap_check ${ENSCRIPT_OUTPUT_FLAG:=-W}

    CANVAS_DOCS_OUT_DIR=$ZMAP_CHECKOUT_ROOT/$FOOCANVAS_DOC_TARGET
    
    mkdir -p $CANVAS_DOCS_OUT_DIR || zmap_message_exit "Failed creating output directory."
    
    # Because we cd out of this dir we need CANVAS_DOCS_OUT_DIR to be absolute
    # This is how we do that.
    if ! echo $CANVAS_DOCS_OUT_DIR | egrep "(^)/" ; then
	this_dir=$(pwd)
	zmap_cd $CANVAS_DOCS_OUT_DIR
	CANVAS_DOCS_OUT_DIR=$(pwd)
	zmap_cd $this_dir
    fi

    foo_headers_dir=`find $GTK_PREFIX/include -name libfoocanvas`
    
    if [ "x$foo_headers_dir" != "x" ]; then

	this_dir=$(pwd)

	zmap_cd $foo_headers_dir
    
	foo_headers=`find . -name 'foo*.h'`

	touch $CANVAS_DOCS_OUT_DIR/this_links.ini
	touch $CANVAS_DOCS_OUT_DIR/index.shtml

	c=$(date "+%S%N")

	cat > $CANVAS_DOCS_OUT_DIR/this_links.ini <<LINKS
# Auto generated by $0
Include$c=<<INC
	<span class="barhead">Foocanvas Interface</span>
INC
LINKS

cat > $CANVAS_DOCS_OUT_DIR/index.shtml <<SHTML
<!-- Auto generated by $0. Do not edit! -->
<!--#set var="banner" value="ZMap Developer Site"-->
<!--#include virtual="/perl/header"-->
<!--#set var="author" value="edgrif@sanger.ac.uk" -->

<h5>FooCanvas Interface</h5>

<p>Documentation for the FooCanvas Interface generated by enscript.
<pre>
$ENSCRIPT_EXE --color $ENSCRIPT_OUTPUT_FLAG html -Ec -o [FILENAME].shtml [FILENAME]
</pre>
</p>

<ul>
SHTML


# -------------------------------------------------------- #
# Create the html pages and the navigation for them        #
# -------------------------------------------------------- #

	for header in $foo_headers ;
	  do
	  header=$(basename $header)
	  zmap_message_out "Creating enscript doc for FooCanvas file $header"

	  cat > $CANVAS_DOCS_OUT_DIR/$header.shtml <<SHTML
<!-- Automatically added by $0. Do not edit! -->
<!-- $ENSCRIPT_EXE generates most of this file. Do not edit! -->
<!--#include virtual="/perl/header"-->
<!--#set var="author" value="edgrif@sanger.ac.uk" -->
SHTML

	  $ENSCRIPT_EXE --color $ENSCRIPT_OUTPUT_FLAG html -Ec -o - $header >> $CANVAS_DOCS_OUT_DIR/$header.shtml || \
	      zmap_message_exit "Failed to $ENSCRIPT_EXE $header"

	  perl -i -lne 'print if !/HTML/'  $CANVAS_DOCS_OUT_DIR/$header.shtml
	  perl -i -lne 'print if !/HEAD/'  $CANVAS_DOCS_OUT_DIR/$header.shtml
	  perl -i -lne 'print if !/TITLE/' $CANVAS_DOCS_OUT_DIR/$header.shtml
	  perl -i -lne 'print if !/BODY/'  $CANVAS_DOCS_OUT_DIR/$header.shtml
	  perl -i -lne 'print if !/NAME/'  $CANVAS_DOCS_OUT_DIR/$header.shtml

	  cat >> $CANVAS_DOCS_OUT_DIR/this_links.ini <<LINKS
$header=$header.shtml
LINKS

	  cat >> $CANVAS_DOCS_OUT_DIR/index.shtml <<SHTML
  <li><a href="$header.shtml">$header</a></li>
SHTML

	  cat >> $CANVAS_DOCS_OUT_DIR/$header.shtml <<SHTML
<!-- Automatically added by $0. Do not edit! -->

<p>Authors: <a href="mailto:edgrif@sanger.ac.uk">Ed Griffiths &lt;edgrif@sanger.ac.uk&gt;</a>
&amp; <a href="mailto:rds@sanger.ac.uk">Roy Storey &lt;rds@sanger.ac.uk&gt;</a>
<p>
<!--#include virtual="/perl/footer"-->
SHTML


	done
    





cat >> $CANVAS_DOCS_OUT_DIR/index.shtml <<SHTML
<!-- Automatically added by $0. Do not edit! -->

</ul>

<p>Authors: <a href="mailto:edgrif@sanger.ac.uk">Ed Griffiths &lt;edgrif@sanger.ac.uk&gt;</a>
&amp; <a href="mailto:rds@sanger.ac.uk">Roy Storey &lt;rds@sanger.ac.uk&gt;</a>
<p>
<!--#include virtual="/perl/footer"-->
SHTML


	zmap_cd $this_dir
    else
	zmap_message_err "Failed to find libfoocanvas directory under $GTK_PREFIX/include. Is it installed there?"
    fi
fi

exit 0;
