static inline unsigned char msgtag_get_extra(unsigned int tag)
{
    return ((tag >> 24) & 0xff);
}

static inline unsigned long msgtag_get_len(unsigned long tag)
{
    return (tag >> 16) & 0xff;
}

static inline unsigned long msgtag_make(unsigned char len, unsigned short id_op, unsigned char extra)
{
    return (extra << 24) | (len << 16) | id_op;
}