#include "ltfilesmanager.h"

#include <QByteArray>
#include <QDebug>

#include "../core/f1ltcore.h"

LTFilesManager::LTFilesManager(QObject *parent) :
    QObject(parent)
{
}

void LTFilesManager::getLTList()
{
    req = QNetworkRequest(F1LTCore::ltDataList());
    req.setRawHeader("User-Agent","f1lt");

    reply = manager.get(req);

    connect(reply, SIGNAL(finished()), this, SLOT(finishedLTList()));
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SIGNAL(error(QNetworkReply::NetworkError)));
}

void LTFilesManager::getLTFile(QString ltDataFile)
{
    req = QNetworkRequest(F1LTCore::ltDataUrl() + ltDataFile);
    req.setRawHeader("User-Agent","f1lt");

    reply = manager.get(req);

    connect(reply, SIGNAL(finished()), this, SLOT(finishedLTFile()));
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SIGNAL(error(QNetworkReply::NetworkError)));
    connect(reply, SIGNAL(downloadProgress(qint64,qint64)), this, SIGNAL(downloadProgress(qint64,qint64)));
}

void LTFilesManager::finishedLTList()
{
    QString buf = reply->readAll().constData();
    QStringList ltList = buf.split("<br>");

    disconnect(reply, SIGNAL(finished()), this, SLOT(finishedLTFile()));
    disconnect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SIGNAL(error(QNetworkReply::NetworkError)));
    disconnect(reply, SIGNAL(downloadProgress(qint64,qint64)), this, SIGNAL(downloadProgress(qint64,qint64)));

    emit ltListObtained(ltList);
}

void LTFilesManager::finishedLTFile()
{
    QByteArray buf = reply->readAll();

    disconnect(reply, SIGNAL(finished()), this, SLOT(finishedLTList()));
    disconnect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SIGNAL(error(QNetworkReply::NetworkError)));

    emit ltFileObtained(buf);            
}

void LTFilesManager::cancel()
{
    if (reply)
    {
        reply->close();
        manager.deleteResource(req);
    }
}
