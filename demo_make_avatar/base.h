#pragma once
#ifndef BASE_H
#define BASE_H

#include <QString>
#include <QSize>
#include <QObject>
#include <QImage>

class QNetworkReply;
namespace demo
{

#define SINGLETON_DEFINE(TypeName)				\
static TypeName* GetInstance()					\
{												\
	static TypeName type_instance;				\
	return &type_instance;						\
}												\
												\
TypeName(const TypeName&) = delete;				\
TypeName& operator=(const TypeName&) = delete


	QString GetSavePath();
	QSize GetAvatarSize();
	QString GetFileName(const QString& url);
	QImage MakeRoundAvatar(QString strPath, QSize size);
	QImage MakeRoundAvatar(const QImage& avatar, QSize size);


	class ReplyTimeout : public QObject
	{
		Q_OBJECT

	public:
		ReplyTimeout(QNetworkReply *reply, const int timeout);
		~ReplyTimeout();

	signals:
		void timeout();  // 超时信号 - 供进一步处理
	public:
		bool isTimeout();

		protected slots:
		void TaskRecvProgress(qint64 Done, qint64 Total);
		void TaskSendProgress(qint64 Done, qint64 Total);
		void onTimeOut();
	private:
		void loading();
	private:
		QNetworkReply *reply;
		int m_nTimeout;
		QTimer *m_pTimer;
		bool m_bTimeOut = false;
	};


}

#endif // !BASE_H
