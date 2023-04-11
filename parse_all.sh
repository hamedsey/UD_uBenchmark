#!/usr/bin/env bash

python3 pcap_parser_and_plotter.py -d fresh50/rr/ -p 50 &
python3 pcap_parser_and_plotter.py -d fresh100/rr/ -p 100 &

python3 pcap_parser_and_plotter.py -d fresh50/wrr/ -p 50 &
python3 pcap_parser_and_plotter.py -d fresh100/wrr/ -p 100 &

python3 pcap_parser_and_plotter.py -d fresh50/strict/ -p 50 &
python3 pcap_parser_and_plotter.py -d fresh100/strict/ -p 100 &

python3 pcap_parser_and_plotter.py -d fresh50/inorder_strict/ -p 50 &
python3 pcap_parser_and_plotter.py -d fresh100/inorder_strict/ -p 100 &

python3 pcap_parser_and_plotter.py -d fresh50/inorder_rr/ -p 50 &
python3 pcap_parser_and_plotter.py -d fresh100/inorder_rr/ -p 100 &

python3 pcap_parser_and_plotter.py -d fresh50/inorder_wrr/ -p 50 &
python3 pcap_parser_and_plotter.py -d fresh100/inorder_wrr/ -p 100 &

python3 pcap_parser_and_plotter.py -d fresh50/inorder_only/ -p 50 &
python3 pcap_parser_and_plotter.py -d fresh100/inorder_only/ -p 100 &