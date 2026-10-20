// Copyright 2018 GoldenRiver Technology Co., Ltd. All rights reserved.
#ifndef NBL_SERVICE_H_
#define NBL_SERVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <nbl_fwk/stream.h>
#include <nbl_fwk/err.h>
#include <nbl_fwk/uuid.h>
#include <nbl_fwk/log.h>

/// \nbl_group{ nbl_fwk_hee }

/* Expected limits: should be in sync with kernel settings */
#define NBL_SRV_MAX_BUF_SIZE  4080 ///< Max size of per service buffer
#define NBL_SRV_MAX_BUF_COUNT     32 ///< Max number of per service buffers

#define NBL_SRV_MAX_NAME_LEN    64 ///< Max length of service name path name
#define NBL_SRV_DEFAULT_BUF_NUM 8  ///< Default number of per service buffers

/**
 * \brief Common enumerations of Nebula framework.
 */
enum nbl_common {
	NBL_TIMEOUT_INFINITY = -1, ///< Used for waiting for ever.
	NBL_TIMEOUT_500_MS = 500,
};

/**
 * \brief Defines service common flags.
 */
enum nbl_srv_flag {
	/// Service can only be accessed within its secure domain.
	NBL_SRV_FLAG_SCOPE_SECURE_DOMAIN = 0x1,

	/// Service can be accessed in its secure domain and TEE.
	NBL_SRV_FLAG_SCOPE_TEE = 0x2,

	/// Service can be accessed in its secure domain, HEE and TEE.
	NBL_SRV_FLAG_SCOPE_HEE = 0x3,

	/// Service can be accessed in its secure domain, HEE, TEE and REE.
	NBL_SRV_FLAG_SCOPE_REE = 0x4,

	/// Service scope mask of common flag.
	NBL_SRV_FLAG_SCOPE_MASK = 0x7,
};

/**
 * \brief Service context for service management.
 *
 * \see nbl_srv_create
 * \see nbl_srv_destroy
 */
typedef struct nbl_srv nbl_srv_t;

/**
 * \brief Channel context for channel management.
 *
 * \see nbl_chn_open
 * \see nbl_chn_set_priv
 * \see nbl_chn_get_priv
 * \see nbl_chn_get_peer_uuid
 * \see nbl_chn_write_buf
 * \see nbl_chn_read_buf
 * \see nbl_chn_write_stm
 * \see nbl_chn_read_stm
 * \see nbl_chn_write_stm_sync
 * \see nbl_chn_close
 */
typedef struct nbl_chn nbl_chn_t;

/**
 * \brief Application context for application management.
 *
 * \see nbl_app_create
 * \see nbl_app_reg_srv
 * \see nbl_app_unreg_srv
 * \see nbl_app_main_loop
 * \see nbl_app_stop
 * \see nbl_app_destroy
 */
typedef struct nbl_app nbl_app_t;

/**
 * \brief Service connection handler.
 *
 * The handler is invoked when a client application is connecting to a registered
 * service.
 *
 * \param[in] srv The registered service context.
 * \param[in] peer_uuid The client application's UUID. If the client application
 *            is in REE, peer_uuid will be zero UUID.
 * \return #NBL_NO_ERROR if the connection is acceptable and a #nbl_chn_t
 *         will be created for the connection. Other value will cause the connection
 *         to be closed by Nebula framework.
 */
typedef int (*nbl_conn_handler_t)(nbl_srv_t *srv, nbl_uuid_t peer_uuid);

/**
 * \brief Channel handler that handles message in the form of memory buffer.
 *
 * The handler is invoked when a client is sending message to a registered service.
 * The message in memory buffer use user-defined protocol. And the memory buffer
 * is called message buffer. Message buffer can not transfer handles(e.g. #nbl_shm_t).
 *
 * \param[in] chan The channel context that is created by Nebula framework.
 * \param[in] msg_buf The message buffer pointer.
 * \param[in] msg_size The message buffer size.
 * \return #NBL_NO_ERROR if the message is handled properly. Other value will cause
 *         the connection to be closed by Nebula framework.
 */
typedef int (*nbl_chn_buf_handler_t)(nbl_chn_t *chan, void *msg_buf,
		size_t msg_size);

/**
 * \brief Channel handler that handles new shared memory registration.
 *
 * The handler is invoked when a client is registering shared memory to a service.
 *
 * \param[in] chan The channel context that is created by Nebula framework.
 * \param[in] shm_ptr The pointer of shared memory (#nbl_shm_t).
 * \param[in] shm_perm The permission of shared memory. Either NBL_SHM_PERM_RW or NBL_SHM_PERM_RO.
 *
 */
typedef void (*nbl_chn_shm_handler_t)(nbl_chn_t *chan, nbl_shm_t *shm_ptr,
		uint64_t shm_perm);

/**
 * \brief Channel handler that handles message in the form of stream.
 *
 * The handler is invoked when a client is sending message to a registered service.
 * The message is serialized by #nbl_stm_t. Handles(e.g. #nbl_shm_t)
 * can be transfered with the #nbl_stm_t.
 *
 * \param[in] chan The channel context that is created by Nebula framework.
 * \param[in] stream The #nbl_stm_t that serialize the message.
 * \return #NBL_NO_ERROR if the message is handled properly. Other value will cause
 *         the connection to be closed by Nebula framework.
 */
typedef int (*nbl_chn_stm_handler_t)(nbl_chn_t *chan, nbl_stm_t *stream);

/**
 * \brief Channel open handler.
 *
 * The handler is invoked when a channel is created. User private data can be created
 * in the handler. Then set it to channel's user private area with #nbl_chn_set_priv().
 *
 * \param[in] chan The channel context that is created by Nebula framework.
 *
 * \see nbl_chn_set_priv()
 */
typedef void (*nbl_chn_open_handler_t)(nbl_chn_t *chan);

/**
 * \brief Channel close handler.
 *
 * The handler is invoked when a channel is going to be destroyed. User private
 * data can be retrieved by #nbl_chn_get_priv() in the handler. Then user can
 * destroy the private data that is created in #nbl_chn_open_handler_t.
 *
 * \param[in] chan The channel context that is created by Nebula framework.
 *
 * \see nbl_chn_get_priv()
 */
typedef void (*nbl_chn_close_handler_t)(nbl_chn_t *chan);

/**
 * \brief Nebula service descriptor.
 */
typedef struct nbl_srv_desc {
	/**
	 * Name of service. #NBL_SRV_MAX_NAME_LEN is the max string length(exclude '\0').
	 */
	const char *name;

	/**
	 * Service flags #nbl_srv_flag.
	 */
	uint32_t flags;


	/**
	 * Message buffer count of the service. Max count is #NBL_SRV_MAX_BUF_COUNT.
	 * If you have no idea what count to set, use #NBL_SRV_DEFAULT_BUF_NUM.
	 */
	size_t buf_num;

	/**
	 * Message buffer size of the service. Max buffer size is #NBL_SRV_MAX_BUF_SIZE.
	 */
	size_t buf_size;

	/**
	 * Service connection handler.
	 * \see nbl_conn_handler_t
	 */
	nbl_conn_handler_t conn_handler;

	/**
	 * Channel open handler.
	 * \see nbl_chn_open_handler_t
	 */
	nbl_chn_open_handler_t open_handler;

	/**
	 * Channel stream handler.
	 * \see nbl_chn_stm_handler_t. Can not be set when #buf_handler is set.
	 */
	nbl_chn_stm_handler_t stm_handler;

	/**
	 * Channel buffer handler. Can not be set when #stm_handler is set.
	 * \see nbl_chn_buf_handler_t
	 */
	nbl_chn_buf_handler_t buf_handler;

	/**
	 * Channel shared memory handler. Can not be set when #stm_handler is set.
	 * \see nbl_chn_shm_handler_t
	 */
	nbl_chn_shm_handler_t shm_handler;

	/**
	 * Channel close handler.
	 * \see nbl_chn_close_handler_t
	 */
	nbl_chn_close_handler_t close_handler;

	/**
	 * Service context that to be created.
	 */
	nbl_srv_t *srv;
} nbl_srv_desc_t;

/**
 * \brief Creates a service context.
 *
 * Message buffers are not created in this method. They will be created during
 * main loop.
 *
 * \param[in] desc Service descriptor #nbl_srv_desc_t.
 * \param[out] out_srv Pointer that is used for receiving #nbl_srv_t.
 * \return #NBL_NO_ERROR if #nbl_srv_t is created successfully. If name, out_srv
 *         is NULL, #NBL_ERR_INVALID_ARGS will be returned. If buf_num, buf_size is 0,
 *         #NBL_ERR_INVALID_ARGS will be returned. If buf_num exceeds #NBL_SRV_MAX_BUF_COUNT,
 *         #NBL_ERR_INVALID_ARGS will be returned. If buf_size exceeds #NBL_SRV_MAX_BUF_SIZE,
 *         #NBL_ERR_INVALID_ARGS will be returned. If flags specified wrong scope, #NBL_ERR_NOT_SUPPORTED
 *         will be returned. If there is no memory to create service context, #NBL_ERR_NO_MEMORY
 *         will be returned. If #nbl_chn_stm_handler_t and #nbl_chn_buf_handler_t are set
 *         simultaneously, #NBL_ERR_CONFLICT_ARGS will be returned.
 */
int nbl_srv_create(nbl_srv_desc_t *desc, nbl_srv_t **out_srv);

/**
 * \brief Destroys service context.
 *
 * -# Closes and destroys all channels in the service.
 * -# Unregisters the service from its application context(#nbl_app_t).
 * -# Frees the service context.
 *
 * \param[in] srv Service context pointer.
 */
void nbl_srv_destroy(nbl_srv_t *srv);

/**
 * \brief Creates application context.
 *
 * \return Application context pointer #nbl_app_t if create application context success,
 *         NULL otherwise.
 */
nbl_app_t *nbl_app_create();

/**
 * \brief Registers a service context to an application context.
 *
 * -# Opens port of the service.
 * -# Registers connection handler #nbl_conn_handler_t of the service.
 * -# Add service context to application context.
 *
 * \param[in] app Application context to be registered to.
 * \param[in] srv Service context to be registered.
 * \return #NBL_NO_ERROR if success. #NBL_ERR_INVALID_ARGS if app or srv is NULL.
 * \see nbl_app_unreg_srv
 */
int nbl_app_reg_srv(nbl_app_t *app, nbl_srv_t *srv);

/**
 * \brief Unregisters a service context.
 *
 * -# Closes and destroys all channels in the service.
 * -# Remove the service context from its application context(#nbl_app_t).
 *
 * \param[in] srv The service context that is to be unregistered.
 * \return #NBL_NO_ERROR if success. #NBL_ERR_INVALID_ARGS if srv is NULL.
 * \see nbl_app_reg_srv
 */
int nbl_app_unreg_srv(nbl_srv_t *srv);

/**
 * \brief Main event loop of Nebula framework.
 *
 * The main event loop will wait for events and then invoke the handlers of the
 * events.
 *
 * \param[in] app Application context.
 * \return #NBL_NO_ERROR if main loop is successfully stopped. #NBL_ERR_INVALID_ARGS
 *         if app is NULL.
 * \see nbl_app_stop
 */
int nbl_app_main_loop(nbl_app_t *app);

/**
 * \brief Stops application's main event loop.
 *
 * \param[in] app Application context.
 * \return #NBL_NO_ERROR if success. #NBL_ERR_INVALID_ARGS if app is NULL.
 * \see nbl_app_main_loop
 */
int nbl_app_stop(nbl_app_t *app);

/**
 * \brief Destroys an application context.
 *
 * Frees the application context. Service contexts are
 * not destroyed(#nbl_srv_destroy).
 *
 * \note It must be invoked after #nbl_app_main_loop.
 *
 * \param[in] app Application context.
 */
void nbl_app_destroy(nbl_app_t *app);

/**
 * \brief Opens a channel to a service with its name.
 *
 * \param[in] srv_name Name of service.
 * \param[in] timeout A timeout value in milliseconds. A value of #NBL_TIMEOUT_INFINITY
 *            is an infinite timeout.
 * \param[out] out_channel Pointer that is used for receiving #nbl_chn_t.
 * \return #NBL_NO_ERROR if success. #NBL_ERR_INVALID_ARGS if srv_name or out_channel
 *         is NULL. #NBL_ERR_CHANNEL_CLOSED if the target service refuse to accept.
 */
int nbl_chn_open(const char *srv_name, long timeout, nbl_chn_t **out_channel);

/**
 * \brief Sets user defined data to a channel's private area.
 *
 * Nebula framework will not touch channel's private area. The life-cycle of the
 * data in this area must be managed by user.
 *
 * \param[in] chan The channel context.
 * \param[in] priv Pointer of the user defined data.
 * \see nbl_chn_get_priv()
 */
void nbl_chn_set_priv(nbl_chn_t *chan, void *priv);

/**
 * \brief Gets user defined data from a channel's private area.
 *
 * \param[in] chan The channel context.
 * \return Pointer of the user defined data.
 * \see nbl_chn_set_priv()
 */
void *nbl_chn_get_priv(nbl_chn_t *chan);

/**
 * \brief Get channel peer's UUID.
 *
 * \param[in] chan The channel context.
 * \return UUID of a channel peer if the channel peer is client in HEE/TEE. Zero
 *         UUID otherwise.
 */
nbl_uuid_t nbl_chn_get_peer_uuid(nbl_chn_t *chan);

/**
 * \brief Writes data to channel.
 *
 * This operation is asynchronous and copies data to channel's message send buffer.
 * The data use user-defined protocol.
 *
 * \param[in] chan The channel context.
 * \param[in] data Data pointer.
 * \param[in] data_size Data size.
 * \return #NBL_NO_ERROR if success. #NBL_ERR_INVALID_ARGS if chan or data is NULL.
 */
int nbl_chn_write_buf(nbl_chn_t *chan, void *data, uint32_t data_size);

/**
 * \brief Read data from channel.
 *
 * This operation can be synchronous if timeout is specified #NBL_TIMEOUT_INFINITY.
 * Otherwise the operation is asynchronous. The operation reads data from channel's
 * message receive buffer and copies the data to a user defined buffer.
 *
 * \param[in] chan The channel context.
 * \param[out] data Data pointer.
 * \param[in] data_size Size of the data that is to be read.
 * \param[in] timeout A timeout value in milliseconds. A value of #NBL_TIMEOUT_INFINITY
 *            is an infinite timeout.
 * \return #NBL_NO_ERROR if success. #NBL_ERR_INVALID_ARGS if chan or data is NULL.
 *         #NBL_ERR_TIMED_OUT if timeout occurs.
 * \note The data_size must be equal with or larger than the size of data that is
 *       in channel's message receive buffer.
 */
int nbl_chn_read_buf(nbl_chn_t *chan, void *data, uint32_t data_size,
		long timeout);

/**
 * \brief Writes stream to channel.
 *
 * This operation is asynchronous. Stream #nbl_chn_t contains serialized
 * data.
 *
 * \param[in] chan The channel context.
 * \param[in] stream Stream which contains serialized data.
 * \return #NBL_NO_ERROR if success. #NBL_ERR_INVALID_ARGS if chan or stream is NULL.
 */
int nbl_chn_write_stm(nbl_chn_t *chan, nbl_stm_t *stream);

/**
 * \brief Reads stream from channel.
 *
 * This operation can be synchronous if timeout is specified #NBL_TIMEOUT_INFINITY.
 * Otherwise this operation is asynchronous. The operation reads data from channel's
 * message receive buffer and serializes the data to a specified stream.
 *
 * \param[in] chan The channel context.
 * \param[in] stream Stream that is used for receive serialized data.
 * \param[in] timeout A timeout value in milliseconds. A value of #NBL_TIMEOUT_INFINITY
 *            is an infinite timeout.
 * \return #NBL_NO_ERROR if success. #NBL_ERR_INVALID_ARGS if chan or stream is NULL.
 *         #NBL_ERR_TIMED_OUT if timeout occurs.
 */
int nbl_chn_read_stm(nbl_chn_t *chan, nbl_stm_t *stream, long timeout);

/**
 * \brief Writes stream to channel and wait for response stream.
 *
 * This operation is synchronous. Stream #nbl_chn_t contains serialized
 * data.
 *
 * \param[in] chan The channel context.
 * \param[in] request The stream that is used for writing data to channel.
 * \param[in] response The stream that is usde for reading data from channel.
 * \param[in] timeout A timeout value in milliseconds. A value of #NBL_TIMEOUT_INFINITY
 *            is an infinite timeout.
 * \return #NBL_NO_ERROR if success. #NBL_ERR_INVALID_ARGS if any of chan, request
 *         and response is NULL. #NBL_ERR_TIMED_OUT if timeout occurs.
 */
int nbl_chn_write_stm_sync(nbl_chn_t *chan, nbl_stm_t *request,
		nbl_stm_t *response, long timeout);

/**
 * \brief Closes channel.
 *
 * \param[in] chan The channel context.
 */
void nbl_chn_close(nbl_chn_t *chan);

/**
 * \brief Reference to a TA instance
 */
typedef int nbl_ta_handle_t;

/**
 * \brief Launch a TA instance with lifecycle management
 *
 * The launched TA instance is reference counted by the TA reference handle. If ALL reference
 * is closed, then the TA instance will be shutted down. Please note TA will NOT shutdown
 * immediately, it will complete ALL it's pending requests and return from nbl_app_main_loop().
 *
 * If TA is blocking in channel handler callback, and unable to leave nbl_app_main_loop().
 * It'll be killed after waiting for 5 seconds.
 *
 * If timeout_ms is great than zero, this call will block until TA invokes nbl_app_main_loop().
 * If this call returns successfully, then it is safe to call nbl_chn_open() immediately.
 *
 * If timeout_ms is zero, this call will return immediately after TA is launched. This is usually
 * used for launching TA that doing some operations and then exit immediately.
 *
 * \param[in] uuid The UUID of target TA.
 * \param[in] timeout_ms Timeout for waiting TA enter main loop.
 * \param[out] out_handle TA reference handle.
 * \return #NBL_NO_ERROR if success.
 *         #NBL_ERR_INVALID_ARGS out_handle is NULL.
 *         #NBL_ERR_NOT_FOUND if target TA not found.
 *         #NBL_ERR_BUSY if TA is shuting down and not able to be launched again. Please
 *                        Wait for a while and try again.
 *         #NBL_ERR_TIMED_OUT if timeout occurs
 */
int nbl_ta_open(nbl_uuid_t uuid, long timeout_ms, nbl_ta_handle_t *out_handle);

/**
 * \brief close a TA reference handle
 *
 * \param[in] handle TA reference handle to be closed.
 */
void nbl_ta_close(nbl_ta_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* NBL_SERVICE_H_ */
