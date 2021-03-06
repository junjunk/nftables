#ifndef NFTABLES_SOCKET_H
#define NFTABLES_SOCKET_H

//#include <parser.h>

/**
 * struct rt_template - template for routing expressions
 *
 * @token:	parser token for the expression
 * @dtype:	data type of the expression
 * @len:	length of the expression
 * @byteorder:	byteorder
 */
struct socket_template {
	const char		*token;
	const struct datatype	*dtype;
	unsigned int		len;
	enum byteorder		byteorder;
};

extern struct expr *socket_expr_alloc(const struct location *loc,
				    enum nft_socket_keys key);

#endif /* NFTABLES_SOCKET_H */
