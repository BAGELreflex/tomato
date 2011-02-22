/*

	Tomato Firmware
	Copyright (C) 2006-2008 Jonathan Zarate
	Copyright (C) 2011 Deon 'PrinceAMD' Thomas 
	rate limit & connection limit by conanxu
*/

#include "rc.h"

#include <sys/stat.h>
static const char *qoslimitfn = "/etc/qoslimit";

/*int  chain
1 = MANGLE
2 = NAT
*/
void ipt_qoslimit(int chain)
{
	char *buf;
	char *g;
	char *p;
	char *ibw,*obw;//bandwidth
	char *seq;//mark number
	char *ipaddr;//ip address
	char *dlrate,*dlceil;//guaranteed rate & maximum rate for download
	char *ulrate,*ulceil;//guaranteed rate & maximum rate for upload
	char *priority;//priority
	char *lanipaddr; //lan ip address
	char *lanmask; //lan netmask
	char *tcplimit,*udplimit;//tcp connection limit & udp packets per second
	char *laninface; // lan interface
	int priority_num;
	int i;

	//qos1 is enable
	if (!nvram_get_int("new_qoslimit_enable")) return;
	
	//read qos1rules from nvram
	g = buf = strdup(nvram_safe_get("new_qoslimit_rules"));

	ibw = nvram_safe_get("new_qoslimit_ibw");
	obw = nvram_safe_get("new_qoslimit_obw");
	
	lanipaddr = nvram_safe_get("lan_ipaddr");
	lanmask = nvram_safe_get("lan_netmask");
	laninface = nvram_safe_get("lan_ifname");
	
	if (chain == 1)
	{
		ipt_write(
			"-A PREROUTING -j IMQ -i %s --todev 0\n"
			"-A POSTROUTING -j IMQ -o %s --todev 1\n"
			,laninface,laninface);
	}
	
	while (g) {
		/*
		seq<ipaddr<dlrate<dlceil<ulrate<ulceil<priority<tcplimit<udplimit
		*/
		if ((p = strsep(&g, ">")) == NULL) break;
		i = vstrsep(p, "<", &seq, &ipaddr, &dlrate, &dlceil, &ulrate, &ulceil, &priority, &tcplimit, &udplimit);

		priority_num = atoi(priority);
		if ((priority_num < 0) || (priority_num > 5)) continue;

		if (!strcmp(ipaddr,"")) continue;

		if (!strcmp(dlceil,"")) strcpy(dlceil, dlrate);
		if (strcmp(dlrate,"") && strcmp(dlceil, "")) {
			if(chain == 1) {
				if (strlen(ipaddr) != 17 ) {
					if (strchr(ipaddr, '-') != NULL) {
						ipt_write(
							"-A POSTROUTING ! -s %s/%s -m iprange --dst-range  %s -j MARK --set-mark %s\n"
							,lanipaddr,lanmask,ipaddr,seq);
					}
					else {
						ipt_write(
							"-A POSTROUTING ! -s %s/%s -d %s -j MARK --set-mark %s\n"
							,lanipaddr,lanmask,ipaddr,seq);
					}
				}
			}
		}
		
		if (!strcmp(ulceil,"")) strcpy(ulceil, dlrate);
		if (strcmp(ulrate,"") && strcmp(ulceil, "")) {
			if (chain == 1) {
				if (strlen(ipaddr) != 17 ) {
					if (strchr(ipaddr, '-') != NULL) {
						ipt_write(
							"-A PREROUTING -m iprange --src-range %s ! -d %s/%s -j MARK --set-mark %s\n"
							,ipaddr,lanipaddr,lanmask,seq);
					}
					else {
						ipt_write(
							"-A PREROUTING -s %s ! -d %s/%s -j MARK --set-mark %s\n"
							,ipaddr,lanipaddr,lanmask,seq);
					}
				}
				else if (strlen(ipaddr) == 17 ) {
					ipt_write(
					 "-A PREROUTING -m mac --mac-source %s ! -d %s/%s  -j MARK --set-mark %s\n"
					,ipaddr,lanipaddr,lanmask,seq);
				}
			}
		}
		
		// not sure if to use -I or -A but i am using -A as u can SEE!!!!
		if(atoi(tcplimit) > 0){
			if (chain == 2) {
				if (strlen(ipaddr) != 17 ) {
					if (strchr(ipaddr, '-') != NULL) {
						ipt_write(
							"-A PREROUTING -m iprange --src-range %s -p tcp --syn -m connlimit --connlimit-above %s -j DROP\n"
							,ipaddr,tcplimit);
					}
					else	{
						ipt_write(
							"-A PREROUTING -s %s -p tcp --syn -m connlimit --connlimit-above %s -j DROP\n"
							,ipaddr,tcplimit);
					}
				}
				else if (strlen(ipaddr) == 17 ) {
					ipt_write(
						"-A PREROUTING -m mac --mac-source %s -p tcp --syn -m connlimit --connlimit-above %s -j DROP\n"
						,ipaddr,tcplimit);
				}
			}
		}
		if(atoi(udplimit) > 0){
			if (chain == 2) {
				if (strlen(ipaddr) != 17 ) {
					if (strchr(ipaddr, '-') != NULL) {
						ipt_write(
							"-A PREROUTING -m iprange --src-range %s -p udp -m limit --limit %s/sec -j ACCEPT\n"
							,ipaddr,udplimit);
					}
					else	{
						ipt_write(
							"-A PREROUTING -s %s -p udp -m limit --limit %s/sec -j ACCEPT\n"
							,ipaddr,udplimit);
					}
				}
				else if (strlen(ipaddr) == 17 ) {
					ipt_write(
						"-A PREROUTING -m iprange --src-range %s -p udp -m limit --limit %s/sec -j ACCEPT\n"
						,ipaddr,udplimit);
				}
			}
		}
	}
	free(buf);
}

// read nvram into files
void new_qoslimit_start(void)
{
	FILE *tc;
	char *buf;
	char *g;
	char *p;
	char *ibw,*obw;//bandwidth
	char *seq;//mark number
	char *ipaddr;//ip address
	char *dlrate,*dlceil;//guaranteed rate & maximum rate for download
	char *ulrate,*ulceil;//guaranteed rate & maximum rate for upload
	char *priority;//priority
	char *lanipaddr; //lan ip address
	char *lanmask; //lan netmask
	char *tcplimit,*udplimit;//tcp connection limit & udp packets per second
	int priority_num;
	int i;
	int s[6];

	//qos1 is enable
	if (!nvram_get_int("new_qoslimit_enable")) return;

	//read qos1rules from nvram
	g = buf = strdup(nvram_safe_get("new_qoslimit_rules"));

	ibw = nvram_safe_get("new_qoslimit_ibw");
	obw = nvram_safe_get("new_qoslimit_obw");
	
	lanipaddr = nvram_safe_get("lan_ipaddr");
	lanmask = nvram_safe_get("lan_netmask");

	if ((tc = fopen(qoslimitfn, "w")) == NULL) return;

	fprintf(tc,
		"#!/bin/sh\n"
		"ip link set imq0 up\n"
		"ip link set imq1 up\n"
		"\n"
		"tc qdisc del dev imq0 root 2>/dev/null\n"
		"tc qdisc del dev imq1 root 2>/dev/null\n"
		"tc qdisc del dev br0 root 2>/dev/null\n" //fix me [why should mac get filter here??]
		"\n"
		"TCAM=\"tc class add dev br0\"\n" //fix me
		"TFAM=\"tc filter add dev br0\"\n" //fix me
		"TQAM=\"tc qdisc add dev br0\"\n" //fix me
		"\n"
		"TCA=\"tc class add dev imq1\"\n"
		"TFA=\"tc filter add dev imq1\"\n"
		"TQA=\"tc qdisc add dev imq1\"\n"
		"\n"
		"SFQ=\"sfq perturb 10\"\n"
		"\n"
		"TCAU=\"tc class add dev imq0\"\n"
		"TFAU=\"tc filter add dev imq0\"\n"
		"TQAU=\"tc qdisc add dev imq0\"\n"
		"\n"
		"tc qdisc add dev imq1 root handle 1: htb\n"
		"tc class add dev imq1 parent 1: classid 1:1 htb rate %skbit\n"
		"\n"
		"tc qdisc add dev br0 root handle 1: htb\n"
		"tc class add dev br0 parent 1: classid 1:1 htb rate %skbit\n"
		"\n"
		"\n"
		"tc qdisc add dev imq0 root handle 1: htb\n"
		"tc class add dev imq0 parent 1: classid 1:1 htb rate %skbit\n"
		,ibw,ibw,obw
	);
	
	while (g) {
		/*
		seq<ipaddr<dlrate<dlceil<ulrate<ulceil<priority<tcplimit<udplimit
		*/
		if ((p = strsep(&g, ">")) == NULL) break;
		i = vstrsep(p, "<", &seq, &ipaddr, &dlrate, &dlceil, &ulrate, &ulceil, &priority, &tcplimit, &udplimit);

		priority_num = atoi(priority);
		if ((priority_num < 0) || (priority_num > 5)) continue;

		if (!strcmp(ipaddr,"")) continue;

		if (!strcmp(dlceil,"")) strcpy(dlceil, dlrate);
		if (strcmp(dlrate,"") && strcmp(dlceil, "")) {
			if (strlen(ipaddr) != 17 ) {
				fprintf(tc,
					"$TCA parent 1:1 classid 1:%s htb rate %skbit ceil %skbit prio %s\n"
					"$TQA parent 1:%s handle %s: $SFQ\n"
					"$TFA parent 1:0 prio %s protocol ip handle %s fw flowid 1:%s\n"
					"\n"
					,seq,dlrate,dlceil,priority
					,seq,seq
					,priority,seq,seq);
			}
			else if (strlen(ipaddr) == 17 ) {
				sscanf(ipaddr, "%02X:%02X:%02X:%02X:%02X:%02X",&s[0],&s[1],&s[2],&s[3],&s[4],&s[5]);
				
				fprintf(tc,
					"$TCAM parent 1:1 classid 1:%s htb rate %skbit ceil %skbit prio %s\n"
					"$TQAM parent 1:%s handle %s: $SFQ\n"
					"$TFAM parent 1:0 protocol ip prio %s u32 match u16 0x0800 0xFFFF at -2 match u32 0x%02X%02X%02X%02X 0xFFFFFFFF at -12 match u16 0x%02X%02X 0xFFFF at -14 flowid 1:%s\n"
					"\n"
					,seq,dlrate,dlceil,priority
					,seq,seq
					,priority,s[2],s[3],s[4],s[5],s[0],s[1],seq);
			}
		}
		
		if (!strcmp(ulceil,"")) strcpy(ulceil, dlrate);
		if (strcmp(ulrate,"") && strcmp(ulceil, "")) {
			fprintf(tc,
				"$TCAU parent 1:1 classid 1:%s htb rate %skbit ceil %skbit prio %s\n"
				"$TQAU parent 1:%s handle %s: $SFQ\n"
				"$TFAU parent 1:0 prio %s protocol ip handle %s fw flowid 1:%s\n"
				"\n"
				,seq,ulrate,ulceil,priority
				,seq,seq
				,priority,seq,seq);
		}
	}
	free(buf);

	fclose(tc);
	chmod(qoslimitfn, 0700);
	
	//fake start
	eval((char *)qoslimitfn, "start");
}

void new_qoslimit_stop(void)
{
	FILE *f;
	char *s = "/tmp/qoslimittc_stop.sh";

	if ((f = fopen(s, "w")) == NULL) return;

	fprintf(f,
		"#!/bin/sh\n"
		"tc qdisc del dev imq1 root\n"
		"tc qdisc del dev imq0 root\n"
		"tc qdisc del dev br0 root\n" //fix me
		"\n"
	);

	fclose(f);
	chmod(s, 0700);
	//fake stop
	eval((char *)s, "stop");
}
/*

PREROUTING (mn) ----> x ----> FORWARD (f) ----> + ----> POSTROUTING (n)
           QD         |                         ^
                      |                         |
                      v                         |
                    INPUT (f)                 OUTPUT (mnf)


*/
