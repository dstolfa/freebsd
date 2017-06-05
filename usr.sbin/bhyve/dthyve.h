#ifndef _DTHYVE_H_
#define _DTHYVE_H_

#ifdef DTVIRT

struct uuid;

int	dthyve_open(void);
int	dthyve_register_provider(const char *, const char *);
int	dthyve_unregister_provider(struct uuid *);
int	dthyve_probe_create(struct uuid *, const char *,
   	    const char *, const char *);
void	dthyve_cleanup(void);

#endif /* DTVIRT */

#endif /* _DTHYVE_H_ */
