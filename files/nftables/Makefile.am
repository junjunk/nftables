pkgsysconfdir = ${sysconfdir}/nftables
dist_pkgsysconf_DATA =	all-in-one.nft		\
			arp-filter.nft		\
			bridge-filter.nft	\
			inet-filter.nft		\
			ipv4-filter.nft		\
			ipv4-mangle.nft		\
			ipv4-nat.nft		\
			ipv4-raw.nft		\
			ipv6-filter.nft		\
			ipv6-mangle.nft		\
			ipv6-nat.nft		\
			ipv6-raw.nft

install-data-hook:
	${SED} -i 's|@sbindir[@]|${sbindir}/|g' ${DESTDIR}${pkgsysconfdir}/*
