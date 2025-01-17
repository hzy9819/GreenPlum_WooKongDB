#!/usr/bin/env bash

gpsrc=$GPSRC
echo 'We are going to build gpdb from the source code under '$gpsrc','
echo 'If this is not correct, please set the right path for gpsrc in gpbuild.sh'
gptmp=$GPCOMPILE
gpcptmp=$GPCOMPILEHOME
gpbin=$GPBIN
gpcpbin=$GPBINHOME

while getopts ":f:d:b:t:" opts; do
    case $opts in
        f) first=$OPTARG ;;
        d) deploy=$OPTARG ;;
        b) debug=$OPTARG ;;
        t) copy_tmp=$OPTARG ;;
        ?) ;;
    esac
done

gpstate=$(gpstate | grep "Master instance")

if [[ $gpstate =~ "Active" ]]
then
  echo 'gp is running, try to stop it...'
  sleep 5
  gpstop -M immediate -a
fi

if [ $first = "y" -o $first = "Y" ]
then
  make clean
  rm -rf $gptmp
  cp -r $gpsrc $gptmp
else
  rm -f $gptmp/gpAux/gpdemo/demo_cluster.sh
  rm -rf $gptmp/src
  #rm -rf $gptmp/tdsqlcontrib
  rm -f $gptmp/GNUmakefile.in
  cp $gpsrc/gpAux/gpdemo/demo_cluster.sh $gptmp/gpAux/gpdemo/
  cp -r $gpsrc/src $gptmp/
  #cp -r $gpsrc/tdsqlcontrib $gptmp/
  cp $gpsrc/GNUmakefile.in $gptmp/GNUmakefile.in
  cd $gptmp
fi

if [ $debug = "y" -o $debug = "Y" ]
then
  echo "compile in debug mode."
  ./configure --prefix=$gpbin --with-perl --with-gssapi --with-python --with-libxml --with-includes=/usr/include --enable-debug --enable-cassert --disable-orca --without-zstd CFLAGS='-O0 -g3'
else
  echo "compile in release mode."
  ./configure --prefix=$gpbin --with-perl --with-gssapi --with-python --with-libxml --with-includes=/usr/include --disable-cassert --disable-orca --disable-pxf --disable-gpfdisk --without-zstd CFLAGS='-g3 -O3' CXXFLAGS='-g3 -O3'
fi
sed -i 's/LIBS = -lbz2 -lxml2 -lrt -lgssapi_krb5 -lz -lreadline -lrt -lcrypt -ldl -lm  -lcurl/LIBS = -lbz2 -lxml2 -lrt -lgssapi_krb5 -lz -lreadline -lrt -lcrypt -ldl -lm  -lcurl  -lzstd -llz4 -lsnappy -lpthread -lprotobuf -lrocksdb/g' ./src/Makefile.global
make -j8
make -j8 install
source $gpbin/greenplum_path.sh

for seg in gp-seg1 gp-seg2 gp-seg3
do
  if [ $copy_tmp = "y" -o $copy_tmp = "Y" ]
  then
	echo "try to delete the old tmp code in $seg."
	ssh $seg rm -rf $gptmp
	echo "delete complete."
	echo "try to copy the tmp code in $seg."
	scp -rq $gptmp $seg:$gpcptmp
  fi
  echo "try to delete the bin in $seg."
  ssh $seg rm -rf $gpbin
  echo "delete complete."
  echo "try to copy the bin in $seg."
  scp -rq $gpbin $seg:$gpcpbin

  ssh $seg source $gpbin/greenplum_path.sh
done

gpdatap=$GPPRIMARYHOME
gpmaster=$GPMASTER
gpinit=$GPINIT

if [ $deploy = "y" -o $deploy = "Y" ]
then
  echo 'Deploying demo cluster...'
  sleep 5
  #gpdeletesystem -f
  rm -rf $gpmaster
  rm -rf /tmp/.s.PGSQL.*
  for seg in gp-seg1 gp-seg2 gp-seg3
  do
    ssh $seg rm -rf /data1/gptest/penguindb/gpdata/gpdatap/*
    ssh $seg rm -rf /data1/gptest/penguindb/gpdata/gpdatam/*
    ssh $seg rm -rf /tmp/.s.PGSQL.*
  done
  gpinitsystem -c /data1/gptest/penguindb/conf/gpinitsystem_config -a
  source $gptmp/gpAux/gpdemo/gpdemo-env.sh
  echo 'Demo cluster has been deployed and started.'
else
  echo 'gpdb has been built and installed, you may want to start it with: gpstart'
fi

exit 0
