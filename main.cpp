#include <QCoreApplication>
#include <QHttpServer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSslKey>
#include <QSslserver>
#include <QTemporaryFile>
#include <QWebSocketServer>
#include <QObject>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    // Set up code that uses the Qt event loop here.
    // Call a.quit() or a.exit() to quit the application.
    // A not very useful example would be including
    // #include <QTimer>
    // near the top of the file and calling
    // QTimer::singleShot(5000, &a, &QCoreApplication::quit);
    // which quits the application after 5 seconds.

    // If you do not need a running Qt event loop, remove the call
    // to a.exec() or use the Non-Qt Plain C++ Application template.

    QTemporaryFile *certFile = QTemporaryFile::createNativeFile(":/localhost+2.pem");
    QTemporaryFile *keyFile = QTemporaryFile::createNativeFile(":/localhost+2-key.pem");
    QSslCertificate certificate(certFile,QSsl::Pem);
    QSslKey privateKey(keyFile,QSsl::Rsa,QSsl::Pem);


    QSslConfiguration config;
    config.setLocalCertificate(certificate);
    config.setPrivateKey(privateKey);

    QHttpServer server;
    // server.bind();


    QTcpServer tcpServer;
    // QSslServer tcpServer;
    // tcpServer.setSslConfiguration(config);
    tcpServer.listen(QHostAddress::Any,6444);
    // tcpServer.listen();
    server.bind(&tcpServer);



    server.route("/test/",[](const QString page,const QHttpServerRequest &request){
        qDebug() << "method:" << request.method();
        qDebug() << "body:" << request.body();
        qDebug() << "headers:" <<  request.headers();
        qDebug() << "query:" << request.query().toString();
        return "hello world"+page;
    });


    server.route("/image",[]{
        return QHttpServerResponse::fromFile(":/image.png");
    });

    server.route("/mp4",[]{
        auto response = QHttpServerResponse::fromFile("E:/uwsmb/10.mp4");
        auto headers = response.headers();
        headers.append("Accept-Ranges","bytes");
        response.setHeaders(headers);
        return response;
    });

    server.route("/mp4byresponse",[](const QHttpServerRequest &request){
        QFile file("E:/uwsmb/10.mp4");
        file.open(QIODevice::ReadOnly);
        qint64 start = 0;
        qint64 size = file.size();

        auto reqHeaders = request.headers();
        // foreach (const auto &header, request.headers().toListOfPairs()) {
        //     if(header.first == "Range"){
        //         start = header.second.mid(header.second.indexOf('=')+1).split('-')[0].toLongLong();
        //         break;
        //     }
        // }
        if(reqHeaders.contains("Range")){
            auto range =QString::fromUtf8(reqHeaders.value("Range"));
            start = range.mid(range.indexOf('=')+1).split('-')[0].toLongLong();
        }

        // // Content-Range:bytes 37978112-330370234/330370235
        QByteArray range = QString("bytes %1-%2/%3").arg(start).arg(size-1).arg(size).toUtf8();

        file.seek(start);
        QByteArray array = file.read(file.size() - start);
        file.close();

        auto res = QHttpServerResponse(array,QHttpServerResponse::StatusCode::PartialContent);
        auto resHeaders = res.headers();


        resHeaders.append(QHttpHeaders::WellKnownHeader::ContentType,"video/mp4");
        resHeaders.append("Accept-Ranges","bytes");
        resHeaders.append("Content-Range",range);
        resHeaders.append("Access-Control-Expose-Headers","Content-Range,Accept-Ranges");
        res.setHeaders(resHeaders);

        return res;
    });


    server.route("/mp4byresponder",[](const QHttpServerRequest &request,QHttpServerResponder &responder){
        QFile file("E:/uwsmb/10.mp4");
        file.open(QIODevice::ReadOnly);
        qint64 start = 0;
        qint64 size = file.size();

        auto reqHeaders = request.headers();

        if(reqHeaders.contains("Range")){
            auto range =QString::fromUtf8(reqHeaders.value("Range"));
            start = range.mid(range.indexOf('=')+1).split('-')[0].toLongLong();
        }
        // Content-Range:bytes 37978112-330370234/330370235
        QByteArray range = QString("bytes %1-%2/%3").arg(start).arg(size-1).arg(size).toUtf8();

        QHttpHeaders resHeaders;


        resHeaders.append(QHttpHeaders::WellKnownHeader::ContentType,"video/mp4");
        resHeaders.append("Accept-Ranges","bytes");
        resHeaders.append("Content-Range",range);
        resHeaders.append("Access-Control-Expose-Headers","Content-Range,Accept-Ranges");
        resHeaders.append("Access-Control-Allow-Origin","*");
        file.seek(start);
        QByteArray array = file.read(file.size() - start);
        file.close();
        responder.write(array,resHeaders,QHttpServerResponder::StatusCode::PartialContent);

    });

    server.route("/json",[](){
        QJsonObject jsonObject;
        jsonObject["name"] = "UWillno";
        jsonObject["age"] = 23;
        // return jsonObject;  //content-type: text/javascript
        return QHttpServerResponse(jsonObject);
    });


    server.route("/xml",[](){
        QString xmlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
    <rss version="2.0">
      <channel>
        <item>
          <title>Qt 6.0.2 Released</title>
          <link>https://www.qt.io/blog/qt-6.0.2-released</link>
          <pubDate>Wed, 03 Mar 2021 12:40:43 GMT</pubDate>
        </item>
        <item>
          <title>Qt 6.1 Beta Released</title>
          <link>https://www.qt.io/blog/qt-6.1-beta-released</link>
          <pubDate>Tue, 02 Mar 2021 13:05:47 GMT</pubDate>
        </item>
        <item>
          <title>Qt Creator 4.14.1 released</title>
          <link>https://www.qt.io/blog/qt-creator-4.14.1-released</link>
          <pubDate>Wed, 24 Feb 2021 13:53:21 GMT</pubDate>
        </item>
      </channel>
    </rss>)";

        // return xmlContent;
        return QHttpServerResponse("application/xml",xmlContent.toUtf8());
    });


    server.addAfterRequestHandler(&server, [] (const QHttpServerRequest &req, QHttpServerResponse &resp) {
        Q_UNUSED(req);
        auto headers = resp.headers();
        headers.append("Access-Control-Allow-Origin","*");
        resp.setHeaders(headers);
    });



    QObject::connect(&server,&QHttpServer::newWebSocketConnection,&server,[&]{
        auto socket = server.nextPendingWebSocketConnection().release();
        if(socket){
            socket->sendTextMessage("websocket连接成功");
            socket->setParent(&server);
            auto list = server.findChildren<QWebSocket *>();
            foreach (const auto &s, list) {
                if(s->state() == QAbstractSocket::ConnectedState)
                    s->sendTextMessage("当前ws数量："+ QString::number(list.length()));
            }
            // qDebug() << list;
            QObject::connect(socket,&QWebSocket::textMessageReceived,&server,[socket,&server](QString message){
                auto list = server.findChildren<QWebSocket *>();
                foreach (const auto &s, list) {
                    if(s != socket && s->state() == QAbstractSocket::ConnectedState)
                        s->sendTextMessage(message);
                }
            });
            QObject::connect(socket,&QWebSocket::disconnected,socket,&QWebSocket::deleteLater);
        }
    });

    server.addWebSocketUpgradeVerifier(
        &server, [](const QHttpServerRequest &request) {
            if (request.url().path() == "/ws")
                return QHttpServerWebSocketUpgradeResponse::accept();
            else
                return QHttpServerWebSocketUpgradeResponse::passToNext();
        });


    return a.exec();
}
