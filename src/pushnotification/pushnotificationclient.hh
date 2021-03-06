/*
    Flexisip, a flexible SIP proxy server with media capabilities.
    Copyright (C) 2010-2015  Belledonne Communications SARL, All rights reserved.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef PUSH_NOTIFICATION_CLIENT_H
#define PUSH_NOTIFICATION_CLIENT_H

#include <queue>
#include <vector>
#include <ctime>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include "pushnotificationservice.hh"

class PushNotificationClient {
public:
	PushNotificationClient(const std::string &name, PushNotificationService *service, std::shared_ptr<boost::asio::ssl::context> ctx, const std::string &host, const std::string &port, int maxQueueSize, bool isSecure);
	int sendRequest(const std::shared_ptr<PushNotificationRequest> &req);
	bool isIdle();
protected:
	void handle_resolve(std::shared_ptr< PushNotificationRequest > ctx, const boost::system::error_code &error, boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
	void handle_connect(std::shared_ptr< PushNotificationRequest > ctx, const boost::system::error_code &error, boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
	void handle_handshake(std::shared_ptr< PushNotificationRequest > ctx, const boost::system::error_code &error);
	void handle_write(std::shared_ptr< PushNotificationRequest > ctx, const boost::system::error_code& error, size_t bytes_transferred);
	void handle_read(std::shared_ptr< PushNotificationRequest > ctx, const boost::system::error_code &error, size_t bytes_transferred);
private:
	void send(std::shared_ptr< PushNotificationRequest > req);
	void onEnd();
	void onError(std::shared_ptr< PushNotificationRequest > req, const std::string &msg = "");
	void onSuccess(std::shared_ptr< PushNotificationRequest > req);
	void connect(std::shared_ptr< PushNotificationRequest > req);
	bool next();
private:
	PushNotificationService *mService;
	boost::asio::ip::tcp::resolver mResolver;
	boost::asio::ssl::stream<boost::asio::ip::tcp::socket> mSocket;
	std::shared_ptr<boost::asio::ssl::context> mContext;
	std::queue<std::shared_ptr<PushNotificationRequest> > mRequestQueue;
	std::vector<char> mResponse;
	std::string mName;
	std::string mHost,mPort;
	int mMaxQueueSize;
	time_t mLastUse;
	bool mIsSecure;
};

#endif //PUSH_NOTIFICATION_CLIENT_H
