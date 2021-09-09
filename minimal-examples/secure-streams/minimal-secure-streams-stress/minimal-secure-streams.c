/*
 * lws-minimal-secure-streams
 *
 * Written in 2010-2021 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 *
 * This demonstrates a minimal http client using secure streams api.
 *
 * It visits https://warmcat.com/ and receives the html page there.
 *
 * This example is built two different ways from the same source... one includes
 * the policy everything needed to fulfil the stream directly.  The other -client
 * variant has no policy itself and some other minor init changes, and connects
 * to the -proxy example to actually get the connection done.
 *
 * In the -client build case, the example does not even init the tls libraries
 * since the proxy part will take care of all that.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>

// #define FORCE_OS_TRUST_STORE

/*
 * uncomment to force network traffic through 127.0.0.1:1080
 *
 * On your local machine, you can run a SOCKS5 proxy like this
 *
 * $ ssh -N -D 0.0.0.0:1080 localhost -v
 *
 * If enabled, this also fetches a remote policy that also
 * specifies that all traffic should go through the remote
 * proxy.
 */
// #define VIA_LOCALHOST_SOCKS

static int interrupted, bad = 1, force_cpd_fail_portal,
	   force_cpd_fail_no_internet, test_respmap, test_ots,
	   budget = 1, predicted_good = 1, good, orig_budget;
static unsigned int timeout_ms = 8000;
static lws_state_notify_link_t nl;
struct lws_context *context;
static lws_sorted_usec_list_t sul_timeout; /* for each process to complete */

/*
 * If the -proxy app is fulfilling our connection, then we don't need to have
 * the policy in the client.
 *
 * When we build with LWS_SS_USE_SSPC, the apis hook up to a proxy process over
 * a Unix Domain Socket.  To test that, you need to separately run the
 * ./lws-minimal-secure-streams-proxy test app on the same machine.
 */

#if !defined(LWS_SS_USE_SSPC)
static const char * const default_ss_policy =
	"{"
	  "\"release\":"			"\"01234567\","
	  "\"product\":"			"\"myproduct\","
	  "\"schema-version\":"			"1,"
#if defined(VIA_LOCALHOST_SOCKS)
	  "\"via-socks5\":"                     "\"127.0.0.1:1080\","
#endif

	  "\"retry\": ["	/* named backoff / retry strategies */
		"{\"default\": {"
			"\"backoff\": ["	 "1000,"
						 "2000,"
						 "3000,"
						 "5000,"
						"10000"
				"],"
			"\"conceal\":"		"5,"
			"\"jitterpc\":"		"20,"
			"\"svalidping\":"	"30,"
			"\"svalidhup\":"	"35"
		"}}"
	  "],"
	  "\"certs\": [" /* named individual certificates in BASE64 DER */
		/*
		 * Let's Encrypt certs for warmcat.com / libwebsockets.org
		 *
		 * We fetch the real policy from there using SS and switch to
		 * using that.
		 */
#if !defined(FORCE_OS_TRUST_STORE)
		"{\"dst_root_x3\": \""
	"MIIDSjCCAjKgAwIBAgIQRK+wgNajJ7qJMDmGLvhAazANBgkqhkiG9w0BAQUFADA/"
	"MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT"
	"DkRTVCBSb290IENBIFgzMB4XDTAwMDkzMDIxMTIxOVoXDTIxMDkzMDE0MDExNVow"
	"PzEkMCIGA1UEChMbRGlnaXRhbCBTaWduYXR1cmUgVHJ1c3QgQ28uMRcwFQYDVQQD"
	"Ew5EU1QgUm9vdCBDQSBYMzCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB"
	"AN+v6ZdQCINXtMxiZfaQguzH0yxrMMpb7NnDfcdAwRgUi+DoM3ZJKuM/IUmTrE4O"
	"rz5Iy2Xu/NMhD2XSKtkyj4zl93ewEnu1lcCJo6m67XMuegwGMoOifooUMM0RoOEq"
	"OLl5CjH9UL2AZd+3UWODyOKIYepLYYHsUmu5ouJLGiifSKOeDNoJjj4XLh7dIN9b"
	"xiqKqy69cK3FCxolkHRyxXtqqzTWMIn/5WgTe1QLyNau7Fqckh49ZLOMxt+/yUFw"
	"7BZy1SbsOFU5Q9D8/RhcQPGX69Wam40dutolucbY38EVAjqr2m7xPi71XAicPNaD"
	"aeQQmxkqtilX4+U9m5/wAl0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNV"
	"HQ8BAf8EBAMCAQYwHQYDVR0OBBYEFMSnsaR7LHH62+FLkHX/xBVghYkQMA0GCSqG"
	"SIb3DQEBBQUAA4IBAQCjGiybFwBcqR7uKGY3Or+Dxz9LwwmglSBd49lZRNI+DT69"
	"ikugdB/OEIKcdBodfpga3csTS7MgROSR6cz8faXbauX+5v3gTt23ADq1cEmv8uXr"
	"AvHRAosZy5Q6XkjEGB5YGV8eAlrwDPGxrancWYaLbumR9YbK+rlmM6pZW87ipxZz"
	"R8srzJmwN0jP41ZL9c8PDHIyh8bwRLtTcm1D9SZImlJnt1ir/md2cXjbDaJWFBM5"
	"JDGFoqgCWjBH4d1QB7wCCZAA62RjYJsWvIjJEubSfZGL+T0yjWW06XyxV3bqxbYo"
	"Ob8VZRzI9neWagqNdwvYkQsEjgfbKbYK7p2CNTUQ"
		"\"}"
#endif
	  "],"
	  "\"trust_stores\": [" /* named cert chains */
#if !defined(FORCE_OS_TRUST_STORE)
		"{"
			"\"name\": \"le_via_dst\","
			"\"stack\": ["
				"\"dst_root_x3\""
			"]"
		"}"
#endif
	  "],"
	  "\"s\": ["
#if !defined(LWS_WITH_SS_DIRECT_PROTOCOL_STR)
		/*
		 * "fetch_policy" decides from where the real policy
		 * will be fetched, if present.  Otherwise the initial
		 * policy is treated as the whole, hardcoded, policy.
		 */
		"{\"fetch_policy\": {"
			"\"endpoint\":"		"\"warmcat.com\","
			"\"port\":"		"443,"
			"\"protocol\":"		"\"h1\","
			"\"http_method\":"	"\"GET\","
#if defined(VIA_LOCALHOST_SOCKS)
			"\"http_url\":"		"\"policy/minimal-proxy-socks.json\","
#else
			"\"http_url\":"		"\"policy/minimal-proxy-v4.2-v2.json\","
#endif
			"\"tls\":"		"true,"
			"\"opportunistic\":"	"true,"
#if !defined(FORCE_OS_TRUST_STORE)
			"\"tls_trust_store\":"	"\"le_via_dst\","
#endif
			"\"retry\":"		"\"default\""
#else
	"{\"mintest\": {"
			"\"endpoint\": \"warmcat.com\","
			"\"port\": 443,"
			"\"protocol\": \"h1\","
			"\"http_method\": \"GET\","
			"\"http_url\": \"index.html?uptag=${uptag}\","
			"\"metadata\": [{"
			"	\"uptag\": \"X-Upload-Tag:\""
			"}, {"
			"	\"xctype\": \"X-Content-Type:\""
			"}],"
			"\"tls\": true,"
			"\"opportunistic\": true,"
			"\"retry\": \"default\","
			"\"timeout_ms\": 2000,"
			"\"direct_proto_str\": true,"
			"\"tls_trust_store\": \"le_via_dst\""
#endif
		"}},{"
			/*
			 * "captive_portal_detect" describes
			 * what to do in order to check if the path to
			 * the Internet is being interrupted by a
			 * captive portal.  If there's a larger policy
			 * fetched from elsewhere, it should also include
			 * this since it needs to be done at least after
			 * every DHCP acquisition
			 */
		    "\"captive_portal_detect\": {"
                        "\"endpoint\": \"connectivitycheck.android.com\","
			"\"http_url\": \"generate_204\","
			"\"port\": 80,"
                        "\"protocol\": \"h1\","
                        "\"http_method\": \"GET\","
                        "\"opportunistic\": true,"
                        "\"http_expect\": 204,"
			"\"http_fail_redirect\": true"
                "}}"
	"]}"
;

#endif

typedef struct myss {
	struct lws_ss_handle 		*ss;
	void				*opaque_data;
	/* ... application specific state ... */
	lws_sorted_usec_list_t		sul;
	size_t				amt;

	struct lws_genhash_ctx		hash_ctx;
} myss_t;

static int
create_ss(struct lws_context *cx);

#if !defined(LWS_SS_USE_SSPC)

static const char *canned_root_token_payload =
	"grant_type=refresh_token"
	"&refresh_token=Atzr|IwEBIJedGXjDqsU_vMxykqOMg"
	"SHfYe3CPcedueWEMWSDMaDnEmiW8RlR1Kns7Cb4B-TOSnqp7ifVsY4BMY2B8tpHfO39XP"
	"zfu9HapGjTR458IyHX44FE71pWJkGZ79uVBpljP4sazJuk8XS3Oe_yLnm_DIO6fU1nU3Y"
	"0flYmsOiOAQE_gRk_pdlmEtHnpMA-9rLw3mkY5L89Ty9kUygBsiFaYatouROhbsTn8-jW"
	"k1zZLUDpT6ICtBXSnrCIg0pUbZevPFhTwdXd6eX-u4rq0W-XaDvPWFO7au-iPb4Zk5eZE"
	"iX6sissYrtNmuEXc2uHu7MnQO1hHCaTdIO2CANVumf-PHSD8xseamyh04sLV5JgFzY45S"
	"KvKMajiUZuLkMokOx86rjC2Hdkx5DO7G-dbG1ufBDG-N79pFMSs7Ck5pc283IdLoJkCQc"
	"AGvTX8o8I29QqkcGou-9TKhOJmpX8As94T61ok0UqqEKPJ7RhfQHHYdCtsdwxgvfVr9qI"
	"xL_hDCcTho8opCVX-6QhJHl6SQFlTw13"
	"&client_id="
		"amzn1.application-oa2-client.4823334c434b4190a2b5a42c07938a2d";

#endif

static void
process_timeout(lws_sorted_usec_list_t *sul)
{
	lwsl_err("%s: process timed out\n", __func__);

	exit(1);
}

/* secure streams payload interface */

static lws_ss_state_return_t
myss_rx(void *userobj, const uint8_t *buf, size_t len, int flags)
{

	if (flags & LWSSS_FLAG_PERF_JSON)
		return LWSSSSRET_OK;

#if !defined(LWS_WITH_SS_DIRECT_PROTOCOL_STR)
	myss_t *m = (myss_t *)userobj;
	const char *md_srv = "not set", *md_test = "not set";
	size_t md_srv_len = 7, md_test_len = 7;

	lws_ss_get_metadata(m->ss, "srv", (const void **)&md_srv, &md_srv_len);
	lws_ss_get_metadata(m->ss, "test", (const void **)&md_test, &md_test_len);
	lwsl_ss_user(m->ss, "len %d, flags: %d, srv: %.*s, test: %.*s",
		  (int)len, flags, (int)md_srv_len, md_srv,
		  (int)md_test_len, md_test);

	lwsl_hexdump_ss_info(m->ss, buf, len);
#endif

	/*
	 * If we received the whole message, for our example it means
	 * we are done.
	 */
	if (flags & LWSSS_FLAG_EOM) {
		bad = 0;
	}

	return LWSSSSRET_OK;
}

static lws_ss_state_return_t
myss_tx(void *userobj, lws_ss_tx_ordinal_t ord, uint8_t *buf, size_t *len,
	int *flags)
{
	//myss_t *m = (myss_t *)userobj;

	/* in this example, we don't send stuff */

	return LWSSSSRET_TX_DONT_SEND;
}

static lws_ss_state_return_t
myss_state(void *userobj, void *sh, lws_ss_constate_t state,
	   lws_ss_tx_ordinal_t ack)
{
	myss_t *m = (myss_t *)userobj;
#if defined(LWS_WITH_SS_DIRECT_PROTOCOL_STR)
	const char *md_test = "not set";
	size_t md_test_len = 7;
	int i;
	static const char * imd_test_keys[8] = {
		"server:",
		"content-security-policy:",
		"strict-transport-security:",
		"test-custom-header:",
		"x-xss-protection:",
		"x-content-type-options:",
		"x-frame-options:",
		"x-non-exist:",
		};
#endif

	lwsl_ss_user(m->ss, "%s (%d), ord 0x%x",
		  lws_ss_state_name((int)state), state, (unsigned int)ack);

	switch (state) {
	case LWSSSCS_CREATING:
		return lws_ss_client_connect(m->ss);

	case LWSSSCS_CONNECTING:
		lws_ss_start_timeout(m->ss, timeout_ms);

		if (lws_ss_set_metadata(m->ss, "uptag", "myuptag123", 10))
			/* can fail, eg due to OOM, retry later if so */
			return LWSSSSRET_DISCONNECT_ME;
#if !defined(LWS_WITH_SS_DIRECT_PROTOCOL_STR)
		if (lws_ss_set_metadata(m->ss, "ctype", "myctype", 7))
			/* can fail, eg due to OOM, retry later if so */
			return LWSSSSRET_DISCONNECT_ME;
#else
		if (lws_ss_set_metadata(m->ss, "X-Test-Type1:", "myctype1", 8))
			/* can fail, eg due to OOM, retry later if so */
			return LWSSSSRET_DISCONNECT_ME;
		if (lws_ss_set_metadata(m->ss, "X-Test-Type2:", "myctype2", 8))
			/* can fail, eg due to OOM, retry later if so */
			return LWSSSSRET_DISCONNECT_ME;
		if (lws_ss_set_metadata(m->ss, "Content-Type:", "myctype", 7))
			/* can fail, eg due to OOM, retry later if so */
			return LWSSSSRET_DISCONNECT_ME;
#endif
		break;

	case LWSSSCS_ALL_RETRIES_FAILED:
		/* if we're out of retries, we want to close the app and FAIL */
		interrupted = 1;
		bad = 2;
		break;

	case LWSSSCS_CONNECTED:
#if defined(LWS_WITH_SS_DIRECT_PROTOCOL_STR)
	lwsl_cx_user(context, "get direct metadata");
	for (i = 0; i < 8; i++) {
		md_test = "not set";
		lws_ss_get_metadata(m->ss, imd_test_keys[i],
				    (const void **)&md_test, &md_test_len);
		lwsl_ss_user(m->ss, " test key:[%s], got [%s]",
				    imd_test_keys[i], md_test);
	}
#endif
		break;

	case LWSSSCS_QOS_ACK_REMOTE: /* transaction assertively succeeded */
		lwsl_ss_notice(m->ss, "LWSSSCS_QOS_ACK_REMOTE");
		good++;
		break; /* disconnected will move us on */

	case LWSSSCS_QOS_NACK_REMOTE: /* transaction assertively failed */
		lwsl_ss_notice(m->ss, "LWSSSCS_QOS_NACK_REMOTE");
		break; /* disconnected will move us on */

	case LWSSSCS_DISCONNECTED: /* attempt is over */
		if (budget)
			create_ss(context);
		else
			interrupted = 1;
		return LWSSSSRET_DESTROY_ME;

	case LWSSSCS_TIMEOUT:
		lwsl_ss_notice(m->ss, "LWSSSCS_TIMEOUT");
		bad = 3;
		if (budget)
			create_ss(context);
		else
			interrupted = 1;
		return LWSSSSRET_DESTROY_ME;

	case LWSSSCS_USER_BASE:
		lwsl_ss_notice(m->ss, "LWSSSCS_USER_BASE");
		break;

	default:
		break;
	}

	return LWSSSSRET_OK;
}

#if defined(LWS_WITH_SECURE_STREAMS_BUFFER_DUMP)
static void
myss_headers_dump(void *userobj, const uint8_t *buf, size_t len, int done)
{
	lwsl_cx_user(context, "%lu done: %s", len, done ? "true" : "false");

	lwsl_hexdump_err(buf, len);
}
#endif

static int
create_ss(struct lws_context *cx)
{
	lws_ss_info_t ssi;

	budget--;
	lwsl_cx_notice(cx, "starting");

	/* We're making an outgoing secure stream ourselves */

	memset(&ssi, 0, sizeof(ssi));
	ssi.handle_offset = offsetof(myss_t, ss);
	ssi.opaque_user_data_offset = offsetof(myss_t, opaque_data);
	ssi.rx = myss_rx;
	ssi.tx = myss_tx;
	ssi.state = myss_state;
#if defined(LWS_WITH_SECURE_STREAMS_BUFFER_DUMP)
	ssi.dump = myss_headers_dump;
#endif
	ssi.user_alloc = sizeof(myss_t);
	ssi.streamtype = test_ots ? "mintest-ots" :
			 (test_respmap ? "respmap" : "mintest");

	if (lws_ss_create(cx, 0, &ssi, NULL, NULL, NULL, NULL)) {
		lwsl_cx_err(context, "failed to create ss");
		return -1;
	}

	lwsl_cx_notice(cx, "started");

	return 0;
}


static int
app_system_state_nf(lws_state_manager_t *mgr, lws_state_notify_link_t *link,
		    int current, int target)
{
	struct lws_context *cx = lws_system_context_from_system_mgr(mgr);
#if !defined(LWS_SS_USE_SSPC)

	lws_system_blob_t *ab = lws_system_get_blob(context,
				LWS_SYSBLOB_TYPE_AUTH, 1 /* AUTH_IDX_ROOT */);
	size_t size;
#endif

	/*
	 * For the things we care about, let's notice if we are trying to get
	 * past them when we haven't solved them yet, and make the system
	 * state wait while we trigger the dependent action.
	 */
	switch (target) {

#if !defined(LWS_SS_USE_SSPC)

	/*
	 * The proxy takes responsibility for this stuff if we get things
	 * done through that
	 */

	case LWS_SYSTATE_INITIALIZED: /* overlay on the hardcoded policy */
	case LWS_SYSTATE_POLICY_VALID: /* overlay on the loaded policy */

		if (target != current)
			break;

		if (force_cpd_fail_portal)

			/* this makes it look like we're behind a captive portal
			 * because the overriden address does a redirect */

			lws_ss_policy_overlay(context,
				      "{\"s\": [{\"captive_portal_detect\": {"
				         "\"endpoint\": \"google.com\","
					 "\"http_url\": \"/\","
					 "\"port\": 80"
				      "}}]}");

		if (force_cpd_fail_no_internet)

			/* this looks like no internet, because the overridden
			 * port doesn't have anything that will connect to us */

			lws_ss_policy_overlay(context,
				      "{\"s\": [{\"captive_portal_detect\": {"
					 "\"endpoint\": \"warmcat.com\","
					 "\"http_url\": \"/\","
					 "\"port\": 999"
				      "}}]}");
		break;

	case LWS_SYSTATE_REGISTERED:
		size = lws_system_blob_get_size(ab);
		if (size)
			break;

		/* let's register our canned root token so auth can use it */
		lws_system_blob_direct_set(ab,
				(const uint8_t *)canned_root_token_payload,
				strlen(canned_root_token_payload));
		break;

#endif

	case LWS_SYSTATE_OPERATIONAL:
		if (current == LWS_SYSTATE_OPERATIONAL) {
			create_ss(cx);
		}
		break;
	}

	return 0;
}

static lws_state_notify_link_t * const app_notifier_list[] = {
	&nl, NULL
};

#if defined(LWS_WITH_SYS_METRICS)

static int
my_metric_report(lws_metric_pub_t *mp)
{
	lws_metric_bucket_t *sub = mp->u.hist.head;
	char buf[192];

	do {
		if (lws_metrics_format(mp, &sub, buf, sizeof(buf)))
			lwsl_cx_user(context, "%s: %s\n", __func__, buf);
	} while ((mp->flags & LWSMTFL_REPORT_HIST) && sub);

	/* 0 = leave metric to accumulate, 1 = reset the metric */

	return 1;
}

static const lws_system_ops_t system_ops = {
	.metric_report = my_metric_report,
};

#endif

static void
sigint_handler(int sig)
{
	interrupted = 1;
}

static lws_log_cx_t my_log_cx = {
	.lll_flags	= LLLF_LOG_CONTEXT_AWARE |
			  LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_USER,
	.refcount_cb	= lws_log_use_cx_file,
	.u.emit_cx 	= lws_log_emit_cx_file,
};

int main(int argc, const char **argv)
{
	struct lws_context_creation_info info;
	int n = 0, expected = 0, concurrent = 1;
	char cxname[16], logpath[128];
	const char *p;

	signal(SIGINT, sigint_handler);

	memset(&info, 0, sizeof info);
	lws_cmdline_option_handle_builtin(argc, argv, &info);

	if ((p = lws_cmdline_option(argc, argv, "-c")))
		concurrent = atoi(p);

	if (concurrent < 0 || concurrent > 100)
		return 1;

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		my_log_cx.lll_flags = (uint32_t)(LLLF_LOG_CONTEXT_AWARE | atoi(p));

	lws_strncpy(cxname, "ctx0", sizeof(cxname));

	for (n = 0; n < concurrent - 1; n++) {
		if (fork()) {
#if defined(WIN32)
			Sleep(1);
#else
			usleep(1000);
#endif
			lws_snprintf(cxname, sizeof(cxname), "ctx%d", n + 1);
			break;
		}
	}

	/*
	 * Arrange that each process's context logs to a different file
	 */

	info.log_cx = &my_log_cx;
	info.vhost_name = cxname;
	lws_snprintf(logpath, sizeof(logpath), "/tmp/%s.log", cxname);
	my_log_cx.opaque = (void *)logpath;

	lwsl_user("LWS secure streams test client [-d<verb>]\n");

	/* these options are mutually exclusive if given */

	if (lws_cmdline_option(argc, argv, "--force-portal"))
		force_cpd_fail_portal = 1;

	if (lws_cmdline_option(argc, argv, "--force-no-internet"))
		force_cpd_fail_no_internet = 1;

	if (lws_cmdline_option(argc, argv, "--respmap"))
		test_respmap = 1;

	if (lws_cmdline_option(argc, argv, "--ots"))
		/*
		 * Use a streamtype that relies on the OS trust store for
		 * validation
		 */
		test_ots = 1;

	if ((p = lws_cmdline_option(argc, argv, "--timeout_ms")))
		timeout_ms = (unsigned int)atoi(p);

	if ((p = lws_cmdline_option(argc, argv, "--budget")))
		budget = atoi(p);

	predicted_good = budget;
	orig_budget = budget;

	if ((p = lws_cmdline_option(argc, argv, "--pass-limit")))
		predicted_good = atoi(p);

	info.fd_limit_per_thread = 1 + 26 + 1;
	info.port = CONTEXT_PORT_NO_LISTEN;
#if defined(LWS_SS_USE_SSPC)
	info.protocols = lws_sspc_protocols;
	{
		const char *p;

		/* connect to ssproxy via UDS by default, else via
		 * tcp connection to this port */
		if ((p = lws_cmdline_option(argc, argv, "-p")))
			info.ss_proxy_port = (uint16_t)atoi(p);

		/* UDS "proxy.ss.lws" in abstract namespace, else this socket
		 * path; when -p given this can specify the network interface
		 * to bind to */
		if ((p = lws_cmdline_option(argc, argv, "-i")))
			info.ss_proxy_bind = p;

		/* if -p given, -a specifies the proxy address to connect to */
		if ((p = lws_cmdline_option(argc, argv, "-a")))
			info.ss_proxy_address = p;
	}
#else
	info.pss_policies_json = default_ss_policy;
	info.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS |
		       LWS_SERVER_OPTION_H2_JUST_FIX_WINDOW_UPDATE_OVERFLOW |
		       LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
#endif

#if defined(LWS_WITH_MBEDTLS)

	/* uncomment to force mbedtls to load a system trust store like
	 * openssl does
	 *
	 * info.mbedtls_client_preload_filepath =
	 *		"/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem";
	 */
#endif

	/* integrate us with lws system state management when context created */

	nl.name = "app";
	nl.notify_cb = app_system_state_nf;
	info.register_notifier_list = app_notifier_list;

#if defined(LWS_WITH_SYS_METRICS)
	info.system_ops = &system_ops;
	info.metrics_prefix = "ssmex";
#endif

	/* create the context */

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		goto bail;
	}

	/* timeout for each forked process */

	lws_sul_schedule(context, 0, &sul_timeout, process_timeout,
			 (lws_usec_t)((lws_usec_t)budget *
				       (lws_usec_t)timeout_ms * LWS_US_PER_MS));

#if !defined(LWS_SS_USE_SSPC)
	/*
	 * If we're being a proxied client, the proxy does all this
	 */

	/*
	 * Set the related lws_system blobs
	 *
	 * ...direct_set() sets a pointer, so the thing pointed to has to have
	 * a suitable lifetime, eg, something that already exists on the heap or
	 * a const string in .rodata like this
	 */

	lws_system_blob_direct_set(lws_system_get_blob(context,
				   LWS_SYSBLOB_TYPE_DEVICE_SERIAL, 0),
				   (const uint8_t *)"SN12345678", 10);
	lws_system_blob_direct_set(lws_system_get_blob(context,
				   LWS_SYSBLOB_TYPE_DEVICE_FW_VERSION, 0),
				   (const uint8_t *)"v0.01", 5);

	/*
	 * ..._heap_append() appends to a buflist kind of arrangement on heap,
	 * just one block is fine, otherwise it will concatenate the fragments
	 * in the order they were appended (and take care of freeing them at
	 * context destroy time). ..._heap_empty() is also available to remove
	 * everything that was already allocated.
	 *
	 * Here we use _heap_append() just so it's tested as well as direct set.
	 */

	lws_system_blob_heap_append(lws_system_get_blob(context,
				    LWS_SYSBLOB_TYPE_DEVICE_TYPE, 0),
				   (const uint8_t *)"spacerocket", 11);
#endif

	/* the event loop */

	n = 0;
	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

	lws_sul_cancel(&sul_timeout);
	lws_context_destroy(context);

bail:
	lwsl_user("  good: %d / %d budget, pass limit %d\n", good, orig_budget,
			predicted_good);
	if (good < predicted_good)
		bad = 1;

	if ((p = lws_cmdline_option(argc, argv, "--expected-exit")))
		expected = atoi(p);

	if (bad == expected) {
		lwsl_user("Completed: OK (seen expected %d)\n", expected);
		return 0;
	} else
		lwsl_err("Completed: failed: exit %d, expected %d\n", bad, expected);

	return 1;
}
