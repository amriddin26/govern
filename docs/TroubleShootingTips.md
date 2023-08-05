# Installation Troubleshooting

## Unable to start GridDB Server

1. Confirm that GridDB administrator password has been properly set (e.g. admin).

    Please refer to "Note" in "GridDB Admin user setting" of https://github.com/griddb/docs-en/blob/master/manuals/GridDB_QuickStartGuide.md#Using-source-code .

2. Confirm the cluster/clusterName setting in gs_cluster.json.

    Please refer to "Setting parameters of a node" in https://github.com/griddb/docs-en/blob/master/manuals/GridDB_QuickStartGuide.md#Using-source-code .

3. Confirm that the output of "$ hostname -i" is not 127.0.0.1 but your IP address.
    - Otherwise, check the setting in /etc/hosts.
    
    Please refer to "Network settings" in https://github.com/griddb/docs-en/blob/master/manuals/GridDB_QuickStartGuide.md#Using-source-code .
    
4. After installing RPM or DEB package, if the "environment variable not set" error occurs:
    - Login as "gsadm" user after setting its password. The environment variable will be automatically configured.
    - If you login with the su command, add "-" or "-l" option, for example "$ su - gsadm".
    
    Please refer to "Note" in https://github.com/griddb/docs-en/blob/master/manuals/GridDB_QuickStartGuide.md#Using-RPM .

## Unable to run the operating command

5. If a proxy variable (http_proxy) has been set up, it is necessary to configure the --no-proxy option.  
    - Specify "127.0.0.1" and the address of "$ hostname -i", for example "$ export no_proxy=127.0.0.1,10.0.2.15".
    
    Please refer to "Note" in "Starting/stopping" of https://github.com/griddb/docs-en/blob/master/manuals/GridDB_QuickStartGuide.md#Using-source-code .

## Unable to run client library like Java

6. The firewall might be the cause. Open the required port number to allow connections through the firewall.  

    (NoSQL Interface)  
    - Example for CentOS: $ firewall-cmd --zone=public --add-port=31999/udp
    - Example for Ubuntu: $ ufw allow 31999/udp  
    
    (SQL Interface)  
    - Example for CentOS: $ firewall-cmd --zone=public --add-port=41999/udp
    - Example for Ubuntu: $ ufw allow 41999/udp
    
    Please refer to "/transaction/notificationPort" and "sql/notificationPort" in "Cluster definition file" of https://github.com/griddb/docs-en/blob/master/manuals/GridDB_FeaturesReference.md#parameter .

## Using the environment where multicast is not supported like AWS, Azure

7. Use Fixed list method or Provider method insted of Multicast method (default) as GridDB cluster configuration.

    Please refer to "Cluster configuration methods" in https://github.com/griddb/docs-en/blob/master/manuals/GridDB_FeaturesReference.md#structure-of-griddb .
