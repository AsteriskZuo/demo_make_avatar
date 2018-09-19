#include "base.h"
#include <QApplication>
#include <QByteArray>
#include <QCryptographicHash>
#include <QNetworkReply>
#include <QTimer>
#include <QPainter>
#include <QBrush>

QString demo::GetSavePath()
{
	QString path = QApplication::applicationDirPath() + "/output";
	return std::move(path);
}

QSize demo::GetAvatarSize()
{
	return std::move(QSize(30, 30));
}

QString demo::GetFileName(const QString& url)
{
	QByteArray bytes;
	bytes.append(url);
	QCryptographicHash crypto(QCryptographicHash::Md5);
	crypto.addData(bytes);
	return QString(crypto.result().toHex()).toLower();
}

QImage demo::MakeRoundAvatar(QString strPath, QSize size)
{
	QImage dealPix(size, QImage::Format_ARGB32);
	dealPix.fill(Qt::transparent);
	QPainter painter(&dealPix);
	QImage img = QImage(strPath);
	if (!img.isNull())
	{
		img = img.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	}

	QBrush brush(img);//背景图片
	painter.setBrush(brush);
	painter.setPen(Qt::transparent);  //边框色
	painter.setRenderHint(QPainter::Antialiasing);
	painter.drawRoundedRect(img.rect(), 5, 5); //圆角5像素

	return dealPix;
}


demo::ReplyTimeout::ReplyTimeout(QNetworkReply *reply, const int timeout)
	: QObject(reply)
	, reply(reply)
	, m_nTimeout(timeout)
	, m_bTimeOut(false)
{
	m_pTimer = new QTimer(this);
	m_pTimer->setInterval(m_nTimeout);

	connect(m_pTimer, &QTimer::timeout, this, &ReplyTimeout::onTimeOut);
	connect(this, &ReplyTimeout::timeout, reply, &QNetworkReply::abort);
	connect(reply, &QNetworkReply::downloadProgress, this, &ReplyTimeout::TaskRecvProgress);
	connect(reply, &QNetworkReply::uploadProgress, this, &ReplyTimeout::TaskSendProgress);
	connect(reply, &QNetworkReply::finished, m_pTimer, &QTimer::stop);

	m_pTimer->start();
}

demo::ReplyTimeout::~ReplyTimeout()
{

}

bool demo::ReplyTimeout::isTimeout()
{
	return m_bTimeOut;
}

void demo::ReplyTimeout::TaskRecvProgress(qint64 Done, qint64 Total)
{
	loading();
}

void demo::ReplyTimeout::TaskSendProgress(qint64 Done, qint64 Total)
{
	loading();
}

void demo::ReplyTimeout::onTimeOut()
{
	m_pTimer->stop();
	if (reply && reply->isRunning())
	{
		m_bTimeOut = true;
		emit timeout();
	}
}

void demo::ReplyTimeout::loading()
{
	if (m_pTimer->isActive())
	{
		m_pTimer->stop();
	}

	m_pTimer->start();
}

