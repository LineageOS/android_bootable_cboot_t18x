/*
 * Copyright (c) 2016-2019, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef TEGRABL_AUTH_H
#define TEGRABL_AUTH_H

#include <stdbool.h>
#include <stdint.h>
#include <tegrabl_error.h>
#include <tegrabl_sigheader.h>
#include <tegrabl_crypto.h>
#if defined(CONFIG_ENABLE_SECURE_BOOT)
#include <tegrabl_binary_types.h>
#endif

#define TEGRABL_AUTH_MAX_HEADERS 2

/**
 * @brief Stores the information extracted from generic header
 * and information required for all se operation mentioned in
 * generic header.
 */
struct tegrabl_auth_header_info {
	/* Mode of authentication */
	tegrabl_signingtype_t mode;
	/* Size which needs to be validated */
	uint32_t validation_size;
	/* Total size validated */
	uint32_t processed_size;
	/* Size of the binary mentioned in generic header */
	uint32_t binary_size;
	/* AES context for se aes operation */
	struct tegrabl_crypto_aes_context aes_context;
	/* RSA context for se sha and rsa operations */
	struct tegrabl_crypto_rsa_pss_context rsa_pss_context;

#if defined(CONFIG_ENABLE_ECDSA)
	/* Ecdsa context for se sha and ecdsa operations */
	struct tegrabl_crypto_ecdsa_context ecdsa_context;
#endif
};

/**
 * @brief Describes the information stored while authenticating
 * binary.
 */
struct tegrabl_auth_handle {
	/* Binary type */
	uint32_t bin_type;
	/* Allowed modes of authentication for binary */
	uint32_t allowed_modes;
	/* Size of destination buffer */
	uint32_t dest_size;
	/* Total size processed including header */
	uint32_t processed_size;
	/* Size of the binary as per topmost header */
	uint32_t binary_size;
	/* Total remaining size of binary which is to be processed */
	uint32_t remaining_size;
	/* Number of header found till now */
	uint32_t num_headers;
	/* Header number for which binary is being validated */
	uint32_t cur_header;
	/* Safe location to load remaining binary */
	void *safe_dest_location;
	/* Final location of data of processed binary */
	void *dest_location;
	/* Header informations are stored in these */
	struct tegrabl_auth_header_info headers[TEGRABL_AUTH_MAX_HEADERS];
	/* Are oem modes allowed */
	bool allow_oem_modes;
	/* True if binary needs to be validated in one call after
	 * entire binary is loaded, instead of calling crypto apis
	 * per process call.
	 */
	bool short_binary;
	/* True if nvidia header is compulsory for binary */
	bool check_nvidia_header;
};

/**
 * @brief Initialize authentication handle for specified binary.
 *
 * @param bin_type Type of binary to be validated
 * @param dest_addr Destination address of buffer where binary is to be loaded.
 * @param dest_size Max size of destination buffer.
 * @param auth Handle which will be initialized and used in other APIS.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_auth_initiate(uint32_t bin_type,
		void *dest_addr, uint32_t dest_size, struct tegrabl_auth_handle *auth);

/**
 * @brief Finds if any new generic header, applies all the se operations
 * as per the headers found on buffer.
 *
 * @param auth Handle having information about binary and headers.
 * @param buffer Buffer which contains the part of binary which is to be
 * validated.
 * @param buffer_size Size of the buffer.
 * @param check_header Check if header is present in buffer
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_auth_process_block(struct tegrabl_auth_handle *auth,
		void *buffer, uint32_t buffer_size, bool check_header);

/**
 * @brief Returns the size of one generic header.
 *
 * @return non zero value.
 */
uint32_t tegrabl_auth_header_size(void);

/**
 * @brief Returns the size of binary mentioned in header found
 * after header_num - 1 headers.
 *
 * @param auth Handle having information about binary and headers.
 * @param header_num Position of header
 *
 * @return Non-zero value if specified header is found else zero.
 */
uint32_t tegrabl_auth_get_size_from_header(
		struct tegrabl_auth_handle *auth, uint32_t header_num);

/**
 * @brief Finalizes the verification by comparing hashes/signatures
 * as per the SE operation mentioned in headers found.
 *
 * @param auth Handle having information about binary and headers.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_auth_finalize(struct tegrabl_auth_handle *auth);

/**
 * @brief Releases all resources acquired during authenticating.
 *
 * @param auth Handle having information about binary and headers.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_auth_end(struct tegrabl_auth_handle *auth);

/**
 * @brief Sets new destination address in auth handle. If move is true
 * then it will copy data processed till this call to new location and
 * updates the safe dest location. If move is false then it will
 * update safe dest location to this address and processed size will
 * be reset.
 *
 * @param auth Handle having information about binary and headers.
 * @param new_addr New destination location of binary
 * @param move Move previously processed data to new location
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_auth_set_new_destination(
		struct tegrabl_auth_handle *auth, void *new_addr, bool move);

/**
 * @brief Special API for binaries which do not have generic header.
 * verification depends on type of binary and security mode.
 *
 * @param bin_type Type of binary to be verified
 * @param bin Pointer to buffer containing entire binary.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_verify_binary(uint32_t bin_type,
		void *bin);

/**
 * @brief Compute hash with zero sbk and compare with input cmac hash
 *
 * @param buffer Input buffer
 * @param buffer_size Size of buffer
 * @param cmachash Input cmac hash of buffer
 *
 * @return TEGRABL_NO_ERROR if buffer is valid else appropriate error.
 */
tegrabl_error_t tegrabl_verify_cmachash(void *buffer,
		uint32_t buffer_size, void *cmachash);

/**
 * @brief Special API for performing standalone encryption or decryption
 * of given buffer using the SBK key slot.
 * it is used for encryption or decryption of binary based on is_decrypt flag.
 *
 * @param buffer Pointer to buffer contain input data
 * @param buffer size size of data in the buffer.
 * @param output_buffer Pointer to buffer containing cipher data.
 * @param is_decrypt flag to indicat if the operation is encryption or decryption.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_cipher_binary(void *buffer,
	uint32_t buffer_size, void *output_buffer, bool is_decrypt);


#if defined(CONFIG_ENABLE_SECURE_BOOT)
tegrabl_error_t tegrabl_auth_payload(tegrabl_binary_type_t bin_type,
			char *name, void *payload, uint32_t max_size);

uint32_t tegrabl_sigheader_size(void);
#endif

#endif /* TEGRABL_AUTH_H */

