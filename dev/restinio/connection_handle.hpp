/*
	restinio
*/

/*!
	A base class for connection handle.
*/

#pragma once

#include <memory>

#include <restinio/common_types.hpp>
#include <restinio/buffers.hpp>

namespace restinio
{

//
// connection_base_t
//

//! HTTP connection base.
class connection_base_t
	:	public std::enable_shared_from_this< connection_base_t >
{
	public:
		connection_base_t(std::uint64_t id )
			:	m_connection_id{ id }
		{}
		virtual ~connection_base_t() = default;

		//! Get connection id.
		std::uint64_t
		connection_id() const
		{
			return m_connection_id;
		}

		//! Write parts for specified request.
		virtual void
		write_response_parts(
			//! Request id.
			request_id_t request_id,
			//! Resp output flag.
			response_output_flags_t response_output_flags,
			//! parts of a response.
			buffers_container_t bufs ) = 0;

	private:
		//! Id of a connection.
		const std::uint64_t m_connection_id;
};

//! Alias for http connection handle.
using connection_handle_t =
	std::shared_ptr< connection_base_t >;

//
// ws_connection_base_t
//

//! WebSocket connection base.
class ws_connection_base_t
	:	public std::enable_shared_from_this< ws_connection_base_t >
{
	public:
		ws_connection_base_t(std::uint64_t id )
			:	m_connection_id{ id }
		{}
		virtual ~ws_connection_base_t() = default;

		//! Get connection id.
		std::uint64_t
		connection_id() const
		{
			return m_connection_id;
		}

		//! Close connection.
		virtual void
		close() = 0;

		//! Start reading ws-messages.
		virtual void
		init_read() = 0;

		//! Write pieces of outgoing data.
		virtual void
		write_data(
			// TODO: what parameter are needed?
			buffers_container_t bufs ) = 0;

	private:
		//! Id of a connection.
		const std::uint64_t m_connection_id;
};

//! Alias for WebSocket connection handle.
using ws_connection_handle_t =
	std::shared_ptr< ws_connection_base_t >;

} /* namespace restinio */
