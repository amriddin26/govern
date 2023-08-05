# インストール時のトラブルシューティング

## 起動できない場合

1. 管理ユーザ（例：admin）のパスワードが設定されているか確認してください。  

    https://github.com/griddb/docs-ja/blob/master/manuals/GridDB_QuickStartGuide/using-source-code.md の「GridDB管理ユーザの設定」を参照してください。

2. gs_cluster.jsonファイルにクラスタ名が設定されているか確認してください。

    https://github.com/griddb/docs-ja/blob/master/manuals/GridDB_QuickStartGuide/using-source-code.md の「パラメータ設定」を参照してください。

3. 「$ hostname -i」にてホスト名から127.0.0.1以外のIPアドレスが取得できるか確認してください。  
    - 取得できない場合は、/etc/hostsファイルの設定を確認してください。  
    
    https://github.com/griddb/docs-ja/blob/master/manuals/GridDB_QuickStartGuide/using-source-code.md の「ネットワークの設定」を参照してください。

4. RPM/DEBパッケージでインストール後、運用コマンドの実行で環境変数が未設定のエラーが発生した場合
    - OSユーザ gsadmでログインしていない可能性があります。gsadmのパスワードを設定した上でgsadmでログインしてください。環境変数が自動設定されます。
    - もしsuコマンドでログインする場合は、「$ su - gsadm」 のように「-」もしくは「-l」オプションを付けてください。

    https://github.com/griddb/docs-ja/blob/master/manuals/GridDB_QuickStartGuide/using-rpm.md の注意を参照してください。

## 運用コマンドの操作ができない場合

5. プロキシ変数(http_proxy)が設定されている場合には、プロキシ側にアクセスしないようにする必要があります。
    - 「$ export no_proxy=127.0.0.1,10.0.2.15」のように、127.0.0.1と「$ hostname -i」で出力されるIPアドレスを設定してください。  

    https://github.com/griddb/docs-ja/blob/master/manuals/GridDB_QuickStartGuide/using-source-code.md の「起動／停止」の注意を参照してください。

## (Javaなど)クライアント操作ができない場合

6. ファイアウォールが原因かもしれません。ファイアウォールに通信用ポートNoを許可してみてください。

    (NoSQL Interface)
    - CentOS, openSUSEの場合の例： $ firewall-cmd --zone=public --add-port=31999/udp
    - Ubuntuの場合の例： $ ufw allow 31999/udp
    
    (SQL Interface)
    - CentOS, openSUSEの場合の例： $ firewall-cmd --zone=public --add-port=41999/udp
    - Ubuntuの場合の例： $ ufw allow 41999/udp

    https://github.com/griddb/docs-ja/blob/master/manuals/GridDB_FeaturesReference/parameter.md の「クラスタ定義ファイル」における"/transaction/notificationPort"および"sql/notificationPort"を参照してください。

## (AWS、Azureのクラウド環境のように)マルチキャストが利用できない環境の場合

7. デフォルトのマルチキャスト方式ではなく、固定リスト方式もしくはプロバイダ方式のクラスタ構成を使ってください。
    https://github.com/griddb/docs-ja/blob/master/manuals/GridDB_FeaturesReference/structure-of-griddb.md の「クラスタ構成方式」を参照してください。
