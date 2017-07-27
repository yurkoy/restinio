/*
	restinio
*/

/*!
	Websocket.
*/

#pragma once

#include <cstdint>
#include <vector>
#include <list>
#include <stdexcept>

#include <restinio/exception.hpp>

namespace restinio
{

const size_t WEBSOCKET_HEADER_SIZE = 2;
const size_t WEBSOCKET_MAX_PAYLOAD_SIZE_WITHOUT_EXT = 125;
const size_t WEBSOCKET_SHORT_EXT_PAYLOAD_LENGTH = 2;
const size_t WEBSOCKET_LONG_EXT_PAYLOAD_LENGTH = 8;
const size_t WEBSOCKET_SHORT_EXT_LEN_CODE = 126;
const size_t WEBSOCKET_LONG_EXT_LEN_CODE = 127;
const size_t WEBSOCKET_MASKING_KEY_SIZE = 4;


// using byte_t = std::uint8_t;
// using raw_data_t = std::vector< byte_t >;

using byte_t = char;
using raw_data_t = std::string;

enum class opcode_t : std::uint8_t
{
	continuation_frame     = 0x00,
    text_frame             = 0x01,
    binary_frame           = 0x02,
    connection_close_frame = 0x08,
    ping_frame             = 0x09,
    pong_frame             = 0x0A
};

struct websocket_message_t
{

	websocket_message_t()
	{
	}

	websocket_message_t( bool final, opcode_t opcode, size_t payload_len )
	{
		m_header.m_final_flag = final;
		m_header.m_opcode = opcode;



	}

	struct header_t
	{
		bool m_final_flag = true;

		bool m_rsv1_flag = false;
		bool m_rsv2_flag = false;
		bool m_rsv3_flag = false;

		opcode_t m_opcode = opcode_t::continuation_frame;

		bool m_mask_flag = false;

		std::uint8_t m_payload_len = 0;
	};

	struct ext_payload_len_t
	{
		std::uint64_t m_value = 0;
	};

	struct masking_key_t
	{
		std::uint32_t m_value = 0;
	};

	std::uint64_t
	payload_len() const
	{
		return m_header.m_mask_flag? m_ext_payload.m_value: m_header.m_payload_len;
	}

	header_t m_header;
	ext_payload_len_t m_ext_payload;
	masking_key_t m_masking_key;
};

namespace impl
{

//! Data with expected size.
struct expected_data_t
{
	expected_data_t( size_t expected_size )
		: m_expected_size{ expected_size }
	{
		m_loaded_data.reserve( m_expected_size );
	}

	size_t m_expected_size{0};
	raw_data_t m_loaded_data;

	//! Try to add one more byte to loaded data and check loaded data size.
	/*!
		\return true if loaded data size equals expected size.
		\return false otherwise.
	*/
	bool
	add_byte_and_check_size( byte_t byte )
	{
		if( m_loaded_data.size() == m_expected_size )
			throw exception_t("Cannot add one more bytes to expected data.");

		m_loaded_data.push_back(byte);

		return m_loaded_data.size() == m_expected_size;
	}

	void
	reset( size_t expected_size )
	{
		m_expected_size = expected_size;
		m_loaded_data.clear();
		m_loaded_data.reserve( expected_size );
	}
};

}

class ws_parser_t
{
	public:

		ws_parser_t()
		{
		}

		~ws_parser_t()
		{
		}

		size_t
		parser_execute( const char * data, size_t size )
		{
			size_t parsed_bytes = 0;

			while( parsed_bytes < size &&
				m_current_state != state_t::header_parsed )
			{
				byte_t byte = data[parsed_bytes];

				process_byte( byte );

				parsed_bytes++;
			}

			return parsed_bytes;
		}

		bool
		header_parsed() const
		{
			return m_current_state == state_t::header_parsed;
		}

		void
		reset()
		{
			m_current_state = state_t::waiting_for_first_2_bytes;
			m_current_msg = websocket_message_t();
			m_expected_data.reset( WEBSOCKET_HEADER_SIZE );
		}

		const websocket_message_t &
		current_message() const
		{
			return m_current_msg;
		}

	private:

		impl::expected_data_t m_expected_data{ WEBSOCKET_HEADER_SIZE };

		websocket_message_t m_current_msg;

		enum class state_t
		{
			waiting_for_first_2_bytes,
			waiting_for_ext_len,
			waiting_for_mask_key,
			header_parsed
		};

		state_t m_current_state = state_t::waiting_for_first_2_bytes;

		void
		process_byte( byte_t byte )
		{
			if( m_expected_data.add_byte_and_check_size(byte) )
			{
				switch( m_current_state )
				{

				case state_t::waiting_for_first_2_bytes:

					process_header();
					break;

				case state_t::waiting_for_ext_len:

					process_extended_length();
					break;

				case state_t::waiting_for_mask_key:

					process_masking_key();
					break;
				}
			}
		}

		void
		process_header()
		{
			m_current_msg.m_header = parse_header(
				m_expected_data.m_loaded_data );

			size_t payload_len = m_current_msg.m_header.m_payload_len;

			if( payload_len > WEBSOCKET_MAX_PAYLOAD_SIZE_WITHOUT_EXT )
			{
				size_t expected_data_size = payload_len == WEBSOCKET_SHORT_EXT_LEN_CODE?
					WEBSOCKET_SHORT_EXT_PAYLOAD_LENGTH:
					WEBSOCKET_LONG_EXT_PAYLOAD_LENGTH;
					;
				m_expected_data.reset( expected_data_size );

				m_current_state = state_t::waiting_for_ext_len;
			}
			else if( m_current_msg.m_header.m_mask_flag )
			{
				size_t expected_data_size = WEBSOCKET_MASKING_KEY_SIZE;
				m_expected_data.reset( expected_data_size );

				m_current_state = state_t::waiting_for_mask_key;
			}
			else
			{
				size_t expected_data_size = payload_len;
				m_expected_data.reset( expected_data_size );

				m_current_state = state_t::header_parsed;
			}
		}

		void
		process_extended_length()
		{
			m_current_msg.m_ext_payload = parse_ext_payload_len(
				m_current_msg.m_header,
				m_expected_data.m_loaded_data );

			if( m_current_msg.m_header.m_mask_flag )
			{
				size_t expected_data_size = WEBSOCKET_MASKING_KEY_SIZE;
				m_expected_data.reset( expected_data_size );

				m_current_state = state_t::waiting_for_mask_key;
			}
			else
			{
				m_current_state = state_t::header_parsed;
			}
		}

		void
		process_masking_key()
		{
			m_current_msg.m_masking_key = parse_masking_key(
				m_current_msg.m_header,
				m_expected_data.m_loaded_data );

			m_current_state = state_t::header_parsed;
		}

		websocket_message_t::header_t
		parse_header( const raw_data_t & data )
		{
			if( data.size() != 2 )
				throw exception_t( "Incorrect size of raw data: 2 bytes expected." );

			websocket_message_t::header_t header;

			header.m_final_flag = data[0] & 0x80;
			header.m_rsv1_flag = data[0] & 0x40;
			header.m_rsv2_flag = data[0] & 0x20;
			header.m_rsv3_flag = data[0] & 0x10;

			header.m_opcode = static_cast< opcode_t >( data[0] & 0x0F );

			header.m_mask_flag = data[1] & 0x80;
			header.m_payload_len = data[1] & 0x7F;

			return header;
		}

		websocket_message_t::ext_payload_len_t
		parse_ext_payload_len(
			const websocket_message_t::header_t & header,
			const raw_data_t & data )
		{
			websocket_message_t::ext_payload_len_t ext_payload_len;

			if( header.m_payload_len == 126 )
			{
				if( data.size() != 2 )
					throw exception_t(
						"Incorrect size of raw data: 2 bytes expected." );

				ext_payload_len.m_value = data[0] << 8;
				ext_payload_len.m_value |= data[1];
			}
			else if( header.m_payload_len == 127 )
			{
				if( data.size() != 8 )
					throw exception_t(
						"Incorrect size of raw data: 8 bytes expected." );

				auto left_shift_bytes = []( byte_t byte, size_t shift_count )
				{
					return static_cast<std::uint64_t>(byte) << shift_count;
				};

				ext_payload_len.m_value = left_shift_bytes( data[0], 56);
				ext_payload_len.m_value |= left_shift_bytes( data[1], 48);
				ext_payload_len.m_value |= left_shift_bytes( data[2], 40);
				ext_payload_len.m_value |= left_shift_bytes( data[3], 32);
				ext_payload_len.m_value |= left_shift_bytes( data[4], 24);
				ext_payload_len.m_value |= left_shift_bytes( data[5], 16);
				ext_payload_len.m_value |= left_shift_bytes( data[6], 8);
				ext_payload_len.m_value |= data[7];
			}

			return ext_payload_len;
		}

		websocket_message_t::masking_key_t
		parse_masking_key(
			const websocket_message_t::header_t & header,
			const raw_data_t & data )
		{
			websocket_message_t::masking_key_t masking_key;

			if( header.m_mask_flag )
			{
				if( data.size() != 4 )
					throw exception_t(
						"Incorrect size of raw data: 4 bytes expected." );

				auto left_shift_bytes = []( byte_t byte, size_t shift_count )
				{
					return static_cast<std::uint32_t>(byte) << shift_count;
				};

				masking_key.m_value |= left_shift_bytes( data[0], 24);
				masking_key.m_value |= left_shift_bytes( data[1], 16);
				masking_key.m_value |= left_shift_bytes( data[2], 8);
				masking_key.m_value |= data[3];
			}

			return masking_key;
		}

};

inline void
mask_unmask_payload( std::uint32_t masking_key, raw_data_t & payload )
{
	uint8_t mask[ 4 ] = { };
	mask[ 0 ] = masking_key & 0xFF;
	mask[ 1 ] = ( masking_key >>  8 ) & 0xFF;
	mask[ 2 ] = ( masking_key >> 16 ) & 0xFF;
	mask[ 3 ] = ( masking_key >> 24 ) & 0xFF;

	for ( size_t index = 0; index < payload.size( ); index++ )
	{
		payload[ index ] ^= mask[ index % 4 ];
	}
}

// raw_data_t
// write_message()

}