static apr_status_t inflate_out_filter(ap_filter_t *f,
                                      apr_bucket_brigade *bb)
{
    int zlib_method;
    int zlib_flags;
    apr_bucket *e;
    request_rec *r = f->r;
    deflate_ctx *ctx = f->ctx;
    int zRC;
    apr_status_t rv;
    deflate_filter_config *c;

    /* Do nothing if asked to filter nothing. */
    if (APR_BRIGADE_EMPTY(bb)) {
        return ap_pass_brigade(f->next, bb);
    }

    c = ap_get_module_config(r->server->module_config, &deflate_module);

    if (!ctx) {

        /* only work on main request/no subrequests */
        if (!ap_is_initial_req(r)) {
            ap_remove_output_filter(f);
            return ap_pass_brigade(f->next, bb);
        }

        /* We can't operate on Content-Ranges */
        if (apr_table_get(r->headers_out, "Content-Range") != NULL) {
            ap_remove_output_filter(f);
            return ap_pass_brigade(f->next, bb);
        }

        /*
         * Let's see what our current Content-Encoding is.
         * Only inflate if gzipped.
         */
        if (check_gzip(r, r->headers_out, r->err_headers_out) == 0) {
            ap_remove_output_filter(f);
            return ap_pass_brigade(f->next, bb);
        }

        /* No need to inflate HEAD or 204/304 */
        if (APR_BUCKET_IS_EOS(APR_BRIGADE_FIRST(bb))) {
            ap_remove_output_filter(f);
            return ap_pass_brigade(f->next, bb);
        }
	// MJP: THis bit is similar
        f->ctx = ctx = apr_pcalloc(f->r->pool, sizeof(*ctx));
        ctx->bb = apr_brigade_create(r->pool, f->c->bucket_alloc);
        ctx->buffer = apr_palloc(r->pool, c->bufferSize);
        ctx->libz_end_func = inflateEnd;
        ctx->validation_buffer = NULL;
        ctx->validation_buffer_length = 0;
	// MJP: same
        zRC = inflateInit2(&ctx->stream, c->windowSize);
	// MJP: same except pass/get brigade swapped
        if (zRC != Z_OK) {
            f->ctx = NULL;
            inflateEnd(&ctx->stream);
            ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
                          "unable to init Zlib: "
                          "inflateInit2 returned %d: URL %s",
                          zRC, r->uri);
            /*
             * Remove ourselves as it does not make sense to return:
             * We are not able to init libz and pass data down the chain
             * compressed.
             */
            ap_remove_output_filter(f);
            return ap_pass_brigade(f->next, bb);
        }
	// MJP: cleanup on other is done 
        /*
         * Register a cleanup function to ensure that we cleanup the internal
         * libz resources.
         */
        apr_pool_cleanup_register(r->pool, ctx, deflate_ctx_cleanup,
                                  apr_pool_cleanup_null);
	// MJP; other places for thsi
        /* these are unlikely to be set anyway, but ... */
        apr_table_unset(r->headers_out, "Content-Length");
        apr_table_unset(r->headers_out, "Content-MD5");
        deflate_check_etag(r, "gunzip");
	//MJP: this is similar
        /* initialize inflate output buffer */
        ctx->stream.next_out = ctx->buffer;
        ctx->stream.avail_out = c->bufferSize;
	// MJP: what is this?
        ctx->inflate_init = 0;
    }
    // MJP: if emtpy
    while (!APR_BRIGADE_EMPTY(bb))
    {
        const char *data;
        apr_bucket *b;
        apr_size_t len;

        e = APR_BRIGADE_FIRST(bb);

        if (APR_BUCKET_IS_EOS(e)) {
            /*
             * We are really done now. Ensure that we never return here, even
             * if a second EOS bucket falls down the chain. Thus remove
             * ourselves.
             */
            ap_remove_output_filter(f);
            /* should be zero already anyway */
            ctx->stream.avail_in = 0;
            /*
             * Flush the remaining data from the zlib buffers. It is correct
             * to use Z_SYNC_FLUSH in this case and not Z_FINISH as in the
             * deflate case. In the inflate case Z_FINISH requires to have a
             * large enough output buffer to put ALL data in otherwise it
             * fails, whereas in the deflate case you can empty a filled output
             * buffer and call it again until no more output can be created.
             */
            flush_libz_buffer(ctx, c, f->c->bucket_alloc, inflate, Z_SYNC_FLUSH,
                              UPDATE_CRC);
            ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r,
                          "Zlib: Inflated %ld to %ld : URL %s",
                          ctx->stream.total_in, ctx->stream.total_out, r->uri);

            if (ctx->validation_buffer_length == VALIDATION_SIZE) {
                unsigned long compCRC, compLen;
                compCRC = getLong(ctx->validation_buffer);
                if (ctx->crc != compCRC) {
                    ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
                                  "Zlib: Checksum of inflated stream invalid");
                    return APR_EGENERAL;
                }
                ctx->validation_buffer += VALIDATION_SIZE / 2;
                compLen = getLong(ctx->validation_buffer);
                if (ctx->stream.total_out != compLen) {
                    ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
                                  "Zlib: Length of inflated stream invalid");
                    return APR_EGENERAL;
                }
            }
            else {
                ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
                              "Zlib: Validation bytes not present");
                return APR_EGENERAL;
            }

            inflateEnd(&ctx->stream);
            /* No need for cleanup any longer */
            apr_pool_cleanup_kill(r->pool, ctx, deflate_ctx_cleanup);

            /* Remove EOS from the old list, and insert into the new. */
            APR_BUCKET_REMOVE(e);
            APR_BRIGADE_INSERT_TAIL(ctx->bb, e);

            /*
             * Okay, we've seen the EOS.
             * Time to pass it along down the chain.
             */
            return ap_pass_brigade(f->next, ctx->bb);
        }

        if (APR_BUCKET_IS_FLUSH(e)) {
            apr_status_t rv;

            /* flush the remaining data from the zlib buffers */
            zRC = flush_libz_buffer(ctx, c, f->c->bucket_alloc, inflate,
                                    Z_SYNC_FLUSH, UPDATE_CRC);
            if (zRC != Z_OK) {
                return APR_EGENERAL;
            }

            /* Remove flush bucket from old brigade anf insert into the new. */
            APR_BUCKET_REMOVE(e);
            APR_BRIGADE_INSERT_TAIL(ctx->bb, e);
            rv = ap_pass_brigade(f->next, ctx->bb);
            if (rv != APR_SUCCESS) {
                return rv;
            }
            continue;
        }

        if (APR_BUCKET_IS_METADATA(e)) {
            /*
             * Remove meta data bucket from old brigade and insert into the
             * new.
             */
            APR_BUCKET_REMOVE(e);
            APR_BRIGADE_INSERT_TAIL(ctx->bb, e);
            continue;
        }

        /* read */
        apr_bucket_read(e, &data, &len, APR_BLOCK_READ);

        /* first bucket contains zlib header */
        if (!ctx->inflate_init++) {
            if (len < 10) {
                ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
                              "Insufficient data for inflate");
                return APR_EGENERAL;
            }
            else  {
                zlib_method = data[2];
                zlib_flags = data[3];
                if (zlib_method != Z_DEFLATED) {
                    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r,
                                  "inflate: data not deflated!");
                    ap_remove_output_filter(f);
                    return ap_pass_brigade(f->next, bb);
                }
                if (data[0] != deflate_magic[0] ||
                    data[1] != deflate_magic[1] ||
                    (zlib_flags & RESERVED) != 0) {
                        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
                                      "inflate: bad header");
                    return APR_EGENERAL ;
                }
                data += 10 ;
                len -= 10 ;
           }
           if (zlib_flags & EXTRA_FIELD) {
               unsigned int bytes = (unsigned int)(data[0]);
               bytes += ((unsigned int)(data[1])) << 8;
               bytes += 2;
               if (len < bytes) {
                   ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
                                 "inflate: extra field too big (not "
                                 "supported)");
                   return APR_EGENERAL;
               }
               data += bytes;
               len -= bytes;
           }
           if (zlib_flags & ORIG_NAME) {
               while (len-- && *data++);
           }
           if (zlib_flags & COMMENT) {
               while (len-- && *data++);
           }
           if (zlib_flags & HEAD_CRC) {
                len -= 2;
                data += 2;
           }
        }

        /* pass through zlib inflate. */
        ctx->stream.next_in = (unsigned char *)data;
        ctx->stream.avail_in = len;

        if (ctx->validation_buffer) {
            if (ctx->validation_buffer_length < VALIDATION_SIZE) {
                apr_size_t copy_size;

                copy_size = VALIDATION_SIZE - ctx->validation_buffer_length;
                if (copy_size > ctx->stream.avail_in)
                    copy_size = ctx->stream.avail_in;
                memcpy(ctx->validation_buffer + ctx->validation_buffer_length,
                       ctx->stream.next_in, copy_size);
                /* Saved copy_size bytes */
                ctx->stream.avail_in -= copy_size;
                ctx->validation_buffer_length += copy_size;
            }
            if (ctx->stream.avail_in) {
                ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r,
                              "Zlib: %d bytes of garbage at the end of "
                              "compressed stream.", ctx->stream.avail_in);
                /*
                 * There is nothing worth consuming for zlib left, because it is
                 * either garbage data or the data has been copied to the
                 * validation buffer (processing validation data is no business
                 * for zlib). So set ctx->stream.avail_in to zero to indicate
                 * this to the following while loop.
                 */
                ctx->stream.avail_in = 0;
            }
        }

        zRC = Z_OK;

        while (ctx->stream.avail_in != 0) {
            if (ctx->stream.avail_out == 0) {

                ctx->stream.next_out = ctx->buffer;
                len = c->bufferSize - ctx->stream.avail_out;

                ctx->crc = crc32(ctx->crc, (const Bytef *)ctx->buffer, len);
                b = apr_bucket_heap_create((char *)ctx->buffer, len,
                                           NULL, f->c->bucket_alloc);
                APR_BRIGADE_INSERT_TAIL(ctx->bb, b);
                ctx->stream.avail_out = c->bufferSize;
                /* Send what we have right now to the next filter. */
                rv = ap_pass_brigade(f->next, ctx->bb);
                if (rv != APR_SUCCESS) {
                    return rv;
                }
            }

            zRC = inflate(&ctx->stream, Z_NO_FLUSH);

            if (zRC == Z_STREAM_END) {
                /*
                 * We have inflated all data. Now try to capture the
                 * validation bytes. We may not have them all available
                 * right now, but capture what is there.
                 */
                ctx->validation_buffer = apr_pcalloc(f->r->pool,
                                                     VALIDATION_SIZE);
                if (ctx->stream.avail_in > VALIDATION_SIZE) {
                    ctx->validation_buffer_length = VALIDATION_SIZE;
                    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r,
                                  "Zlib: %d bytes of garbage at the end of "
                                  "compressed stream.",
                                  ctx->stream.avail_in - VALIDATION_SIZE);
                } else if (ctx->stream.avail_in > 0) {
                           ctx->validation_buffer_length = ctx->stream.avail_in;
                }
                if (ctx->validation_buffer_length)
                    memcpy(ctx->validation_buffer, ctx->stream.next_in,
                           ctx->validation_buffer_length);
                break;
            }

            if (zRC != Z_OK) {
                return APR_EGENERAL;
            }
        }

        apr_bucket_delete(e);
    }

    apr_brigade_cleanup(bb);
    return APR_SUCCESS;
}
