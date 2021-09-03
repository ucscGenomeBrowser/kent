#!/usr/bin/env perl

use warnings;
use strict;

my %fixup = ( 'ITA' => 'Italy',
              'DRC' => 'Democratic Republic of the Congo',
              'FRANCE' => 'France',
              'FRance' => 'France',
              'MAlta' => 'Malta',
              'PAKISTAN' => 'Pakistan',
              'BurkinaFaso' => 'Burkina Faso',
              'HongKong' => 'Hong Kong',
              'SouthAfrica' => 'South Africa',
              'USA-IN' => 'USA',
              'Anhui' => 'China',
              'Beijing' => 'China',
              'Changde' => 'China',
              'Changzhou' => 'China',
              'Chongqing' => 'China',
              'Foshan' => 'China',
              'Fujian' => 'China',
              'Fuzhou' => 'China',
              'Gansu' => 'China',
              'Ganzhou' => 'China',
              'Guangdong' => 'China',
              'Guangxi' => 'China',
              'Guangzhou' => 'China',
              'Hangzhou' => 'China',
              'Harbin' => 'China',
              'Hebei' => 'China',
              'Heilongjiang' => 'China',
              'Henan' => 'China',
              'Hunan' => 'China',
              'Jian' => 'China',
              'Jiangsu' => 'China',
              'Jiangxi' => 'China',
              'Jingzhou' => 'China',
              'Kashgar' => 'China',
              'Jiujiang' => 'China',
              'Liaoning' => 'China',
              'Lishui' => 'China',
              'Lu\'an' => 'China',
              'Meizhou' => 'China',
              'Nan Chang' => 'China',
              'Nanchang' => 'China',
              'Pingxiang' => 'China',
              'Qingdao' => 'China',
              'Shaanxi' => 'China',
              'Shandong' => 'China',
              'Shanghai' => 'China',
              'Shangrao' => 'China',
              'Shaoxing' => 'China',
              'Shenzhen' => 'China',
              'Shulan' => 'China',
              'Sichuan' => 'China',
              'Tianmen' => 'China',
              'Urumqi' => 'China',
              'Weifang' => 'China',
              'Wuhan' => 'China',
              'Xinyu' => 'China',
              'Yichun' => 'China',
              'Yingtan' => 'China',
              'Yunnan' => 'China',
              'Zhejiang' => 'China',
            );

while (<>) {
  s/[\r\n]+$//;
  my $wholeName = $_;
  $wholeName =~ s/[ ',()]//g;
  if (/^([a-z ]+\/|North America\/)?([A-Z][a-zA-Z '_-]+)\//) {
    my $country = $2;
    if (exists $fixup{$country}) {
      $country = $fixup{$country};
    }
    print "$wholeName\t$country\n";
  } else {
    print "$wholeName\t?\n";
  }
}

