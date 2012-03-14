/*
 Flexisip, a flexible SIP proxy server with media capabilities.
 Copyright (C) 2010  Belledonne Communications SARL.

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

#include "agent.hh"
#include "mediarelay.hh"
#include "callstore.hh"
#include "sdp-modifier.hh"
#include "transaction.hh"

#include <vector>
#include <algorithm>

using namespace ::std;

class RelayedCall;

class MediaRelay: public Module, protected ModuleToolbox {
public:
	MediaRelay(Agent *ag);
	~MediaRelay();
	virtual void onLoad(Agent *ag, const ConfigStruct * modconf);
	virtual void onRequest(shared_ptr<SipEvent> &ev);
	virtual void onResponse(shared_ptr<SipEvent> &ev);
	virtual void onIdle();
protected:
	virtual void onDeclare(ConfigStruct * module_config) {
		ConfigItemDescriptor items[] = { { String, "nortpproxy", "SDP attribute set by the first proxy to forbid subsequent proxies to provide relay.", "nortpproxy" }, config_item_end };
		module_config->addChildrenValues(items);
	}
private:
	bool processNewInvite(RelayedCall *c, const shared_ptr<StatefulSipEvent> &ev);
	void process200OkforInvite(RelayedCall *c, msg_t *msg, sip_t *sip);
	CallStore *mCalls;
	MediaRelayServer *mServer;
	string mSdpMangledParam;
	bool mFork;
	static ModuleInfo<MediaRelay> sInfo;
};

class RelayCallHandler;

class RelayedCall: public CallContextBase {
	class RelaySessionTransaction {
	public:
		RelaySessionTransaction() :
				mRelaySession(NULL) {

		}

		RelaySession *mRelaySession;
		map<shared_ptr<Transaction>, shared_ptr<MediaSource>> mTransactions;
	};
public:
	static const int sMaxSessions = 4;
	RelayedCall(MediaRelayServer *server, sip_t *sip) :
			CallContextBase(sip), mServer(server) {
	}

	/*this function is called to masquerade the SDP, for each mline*/
	void newMedia(int mline, string *ip, int *port) {
		if (mline >= sMaxSessions) {
			LOGE("Max sessions per relayed call is reached.");
			return;
		}
		RelaySession *s = mSessions[mline].mRelaySession;
		if (s == NULL) {
			s = mServer->createSession();
			mSessions[mline].mRelaySession = s;
			s->addFront()->set(*ip, *port);
		}

	}

	void backwardTranslate(int mline, string *ip, int *port) {
		if (mline >= sMaxSessions) {
			return;
		}
		RelaySession *s = mSessions[mline].mRelaySession;
		if (s != NULL) {
			*port = s->getFronts().front()->getRelayPort();
			*ip = s->getPublicIp();
		}
	}

	void forwardTranslate(int mline, string *ip, int *port, const std::shared_ptr<Transaction> &transaction) {
		if (mline >= sMaxSessions) {
			return;
		}
		RelaySession *s = mSessions[mline].mRelaySession;
		if (s != NULL) {
			auto it = mSessions[mline].mTransactions.find(transaction);
			if (it != mSessions[mline].mTransactions.end()) {
				*port = it->second->getRelayPort();
			} else {
				LOGE("Can't find transaction %p", transaction.get());
			}
			*ip = s->getPublicIp();
		}
	}

	void addBack(int mline, string *ip, int *port, const std::shared_ptr<Transaction> &transaction) {
		if (mline >= sMaxSessions) {
			return;
		}
		RelaySession *s = mSessions[mline].mRelaySession;
		if (s != NULL) {
			mSessions[mline].mTransactions.insert(pair<shared_ptr<Transaction>, shared_ptr<MediaSource>>(transaction, s->addBack()));

			s->setType((mSessions[mline].mTransactions.size() > 1) ? RelaySession::FrontToBack : RelaySession::All);
		}
	}

	void setBack(int mline, string *ip, int *port, const std::shared_ptr<Transaction> &transaction) {
		if (mline >= sMaxSessions) {
			return;
		}
		RelaySession *s = mSessions[mline].mRelaySession;
		if (s != NULL) {
			auto it = mSessions[mline].mTransactions.find(transaction);
			if (it != mSessions[mline].mTransactions.end()) {
				std::shared_ptr<MediaSource> ms = it->second;
				ms->set(*ip, *port);
			} else {
				LOGE("Can't find transaction %p", transaction.get());
			}
		}
	}

	void removeBack(const std::shared_ptr<OutgoingTransaction> transaction) {
		for (int mline = 0; mline < sMaxSessions; ++mline) {
			RelaySession *s = mSessions[mline].mRelaySession;
			if (s != NULL) {
				auto it = mSessions[mline].mTransactions.find(transaction);
				if (it != mSessions[mline].mTransactions.end()) {
					std::shared_ptr<MediaSource> ms = it->second;
					s->removeBack(ms);
					s->setType((mSessions[mline].mTransactions.size() > 1) ? RelaySession::FrontToBack : RelaySession::All);
				} else {
					LOGE("Can't find transaction %p", transaction.get());
				}
			}
		}
	}

	void freeBack(const std::shared_ptr<OutgoingTransaction> transaction) {
		for (int mline = 0; mline < sMaxSessions; ++mline) {
			RelaySession *s = mSessions[mline].mRelaySession;
			if (s != NULL) {
				auto it = mSessions[mline].mTransactions.find(transaction);
				if (it != mSessions[mline].mTransactions.end()) {
					mSessions[mline].mTransactions.erase(it);
				} else {
					LOGE("Can't find transaction %p", transaction.get());
				}
			}
		}
	}

	void update() {
		mServer->update();
	}

	bool isInactive(time_t cur) {
		time_t maxtime = 0;
		RelaySession *r;
		for (int i = 0; i < sMaxSessions; ++i) {
			time_t tmp;
			r = mSessions[i].mRelaySession;
			if (r && ((tmp = r->getLastActivityTime()) > maxtime))
				maxtime = tmp;
		}
		if (cur - maxtime > 30)
			return true;
		return false;
	}

	~RelayedCall() {
		int i;
		for (i = 0; i < sMaxSessions; ++i) {
			RelaySession *s = mSessions[i].mRelaySession;
			if (s)
				s->unuse();
		}
	}

private:
	RelaySessionTransaction mSessions[sMaxSessions];
	MediaRelayServer *mServer;
};

static bool isEarlyMedia(sip_t *sip) {
	if (sip->sip_status->st_status == 180 || sip->sip_status->st_status == 183) {
		sip_payload_t *payload = sip->sip_payload;
		//TODO: should check if it is application/sdp
		return payload != NULL;
	}
	return false;
}

class RelayCallHandler: public OutgoingTransactionHandler {
public:
	RelayCallHandler(RelayedCall *relayedCall) :
			mRelayedCall(relayedCall) {

	}

	void onNew(const std::shared_ptr<OutgoingTransaction> &transaction) {

	}

	void onEvent(const std::shared_ptr<OutgoingTransaction> &transaction, const std::shared_ptr<StatefulSipEvent> &event) {
		shared_ptr<MsgSip> ms = event->getMsgSip();
		sip_t *sip = ms->getSip();
		if (sip != NULL && sip->sip_status != NULL) {
			if (sip->sip_status->st_status == 487) {
				mRelayedCall->removeBack(transaction);
			} else if (sip->sip_status->st_status == 200 || isEarlyMedia(sip)) {
				SdpModifier *m = SdpModifier::createFromSipMsg(mRelayedCall->getHome(), sip);
				if (m == NULL)
					return;

				m->iterate(bind(&RelayedCall::setBack, mRelayedCall, placeholders::_1, placeholders::_2, placeholders::_3, ref(transaction)));
				delete m;
			}
		}
	}

	void onDestroy(const std::shared_ptr<OutgoingTransaction> &transaction) {
		mRelayedCall->freeBack(transaction); //Free transaction reference
	}
private:
	RelayedCall *mRelayedCall;
};

ModuleInfo<MediaRelay> MediaRelay::sInfo("MediaRelay", "The MediaRelay module masquerades SDP message so that all RTP and RTCP streams go through the proxy. "
		"The RTP and RTCP streams are then routed so that each client receives the stream of the other. "
		"MediaRelay makes sure that RTP is ALWAYS established, even with uncooperative firewalls.");

MediaRelay::MediaRelay(Agent *ag) :
		Module(ag), mServer(0), mFork(false) {
}

MediaRelay::~MediaRelay() {
	if (mCalls)
		delete mCalls;
	if (mServer)
		delete mServer;
}

void MediaRelay::onLoad(Agent *ag, const ConfigStruct * modconf) {
	mCalls = new CallStore();
	mServer = new MediaRelayServer(ag->getBindIp(), ag->getPublicIp());
	mSdpMangledParam = modconf->get<ConfigString>("nortpproxy")->read();

	ConfigStruct *cr = ConfigManager::get()->getRoot();
	ConfigStruct *ma = cr->get<ConfigStruct>("module::Registrar");
	mFork = ma->get<ConfigBoolean>("fork")->read();
}

bool MediaRelay::processNewInvite(RelayedCall *c, const shared_ptr<StatefulSipEvent> &ev) {
	sip_t *sip = ev->getMsgSip()->getSip();
	msg_t *msg = ev->getMsgSip()->getMsg();

	if (sip->sip_from == NULL || sip->sip_from->a_tag == NULL) {
		LOGW("No tag in from !");
		return false;
	}
	SdpModifier *m = SdpModifier::createFromSipMsg(c->getHome(), sip);
	if (m->hasAttribute(mSdpMangledParam.c_str())) {
		LOGD("Invite is already relayed");
		delete m;
		return false;
	}
	if (m) {
		shared_ptr<Transaction> transaction = ev->getTransaction();

		// Create Media
		m->iterate(bind(&RelayedCall::newMedia, c, placeholders::_1, placeholders::_2, placeholders::_3));

		// Add back
		m->iterate(bind(&RelayedCall::addBack, c, placeholders::_1, placeholders::_2, placeholders::_3, ref(transaction)));

		// Translate
		m->iterate(bind(&RelayedCall::forwardTranslate, c, placeholders::_1, placeholders::_2, placeholders::_3, ref(transaction)));

		m->addAttribute(mSdpMangledParam.c_str(), "yes");
		m->update(msg, sip);
		c->update();

		//be in the record-route
		addRecordRoute(c->getHome(), getAgent(), msg, sip);
		c->storeNewInvite(msg);
		delete m;
	}
	return true;
}

void MediaRelay::onRequest(shared_ptr<SipEvent> &ev) {
	shared_ptr<MsgSip> ms = ev->getMsgSip();
	msg_t *msg = ms->getMsg();
	sip_t *sip = ms->getSip();

	RelayedCall *c;

	if (sip->sip_request->rq_method == sip_method_invite) {
		//Create transaction
		shared_ptr<StatefulSipEvent> sev = dynamic_pointer_cast<StatefulSipEvent>(ev);
		shared_ptr<OutgoingTransaction> ot;
		if (sev == NULL) {
			ot = make_shared<OutgoingTransaction>(mAgent);
			sev = make_shared<StatefulSipEvent>(ot, ev->getMsgSip());
			ev = dynamic_pointer_cast<SipEvent>(sev);
		} else {
			ot = dynamic_pointer_cast<OutgoingTransaction>(sev->getTransaction());
		}
		if (mFork) {
			if ((c = static_cast<RelayedCall*>(mCalls->find(getAgent(), sip, true))) == NULL) {
				c = new RelayedCall(mServer, sip);
				if (processNewInvite(c, sev)) {
					mCalls->store(c);
					ot->addHandler(make_shared<RelayCallHandler>(c));
				} else {
					delete c;
				}
			} else {
				processNewInvite(c, sev);
				ot->addHandler(make_shared<RelayCallHandler>(c));
			}
		} else {
			if ((c = static_cast<RelayedCall*>(mCalls->find(getAgent(), sip))) == NULL) {
				c = new RelayedCall(mServer, sip);
				if (processNewInvite(c, sev)) {
					mCalls->store(c);
					ot->addHandler(make_shared<RelayCallHandler>(c));
				} else {
					delete c;
				}
			} else {
				if (c->isNewInvite(sip)) {
					processNewInvite(c, sev);
					ot->addHandler(make_shared<RelayCallHandler>(c));
				} else if (c->getLastForwardedInvite() != NULL) {
					uint32_t via_count = 0;
					for (sip_via_t *via = sip->sip_via; via != NULL; via = via->v_next)
						++via_count;

					// Same vias?
					if (via_count == c->getViaCount()) {
						msg = msg_copy(c->getLastForwardedInvite());
						sip = (sip_t*) msg_object(msg);
						LOGD("Forwarding invite retransmission");
					}
				}
			}
		}
	} else if (sip->sip_request->rq_method == sip_method_bye) {
		if ((c = static_cast<RelayedCall*>(mCalls->find(getAgent(), sip, mFork))) != NULL) {
			mCalls->remove(c);
			delete c;
		}
	}

	ev->setMsgSip(make_shared<MsgSip>(msg, sip));
}

void MediaRelay::process200OkforInvite(RelayedCall *c, msg_t *msg, sip_t *sip) {
	LOGD("Processing 200 Ok");

	if (sip->sip_to == NULL || sip->sip_to->a_tag == NULL) {
		LOGW("No tag in answer");
		return;
	}
	SdpModifier *m = SdpModifier::createFromSipMsg(c->getHome(), sip);
	if (m == NULL)
		return;

	m->iterate(bind(&RelayedCall::backwardTranslate, c, placeholders::_1, placeholders::_2, placeholders::_3));
	m->update(msg, sip);
	c->storeNewResponse(msg);

	delete m;
}

void MediaRelay::onResponse(shared_ptr<SipEvent> &ev) {
	shared_ptr<MsgSip> ms = ev->getMsgSip();
	sip_t *sip = ms->getSip();
	msg_t *msg = ms->getMsg();
	RelayedCall *c;

	if (sip->sip_cseq && sip->sip_cseq->cs_method == sip_method_invite) {
		if (mFork) {
			fixAuthChallengeForSDP(ms->getHome(), msg, sip);
			if (sip->sip_status->st_status == 200 || isEarlyMedia(sip)) {
				if ((c = static_cast<RelayedCall*>(mCalls->find(getAgent(), sip, true))) != NULL) {
					process200OkforInvite(c, msg, sip);
				} else {
					LOGD("Receiving 200Ok for unknown call.");
				}
			}
		} else {
			if (sip->sip_cseq && sip->sip_cseq->cs_method == sip_method_invite) {
				fixAuthChallengeForSDP(ms->getHome(), msg, sip);
				if (sip->sip_status->st_status == 200 || isEarlyMedia(sip)) {
					if ((c = static_cast<RelayedCall*>(mCalls->find(getAgent(), sip))) != NULL) {
						if (sip->sip_status->st_status == 200 && c->isNew200Ok(sip)) {
							process200OkforInvite(c, msg, sip);
						} else if (isEarlyMedia(sip) && c->isNewEarlyMedia(sip)) {
							process200OkforInvite(c, msg, sip);
						} else if (sip->sip_status->st_status == 200 || isEarlyMedia(sip)) {
							LOGD("This is a 200 or 183  retransmission");
							if (c->getLastForwaredResponse() != NULL) {
								msg = msg_copy(c->getLastForwaredResponse());
								sip = (sip_t*) msg_object(msg);
							}
						}
					} else {
						LOGD("Receiving 200Ok for unknown call.");
					}
				}
			}
		}

	}

	ev->setMsgSip(make_shared<MsgSip>(msg, sip));
}

void MediaRelay::onIdle() {
	mCalls->dump();
	mCalls->removeAndDeleteInactives();
}
