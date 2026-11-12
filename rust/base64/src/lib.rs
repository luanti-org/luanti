/*
Copyright (C) 2004-2008 René Nyffenegger
Stricter validation added by the Luanti Contributors.

This source code is provided 'as-is', without any express or implied
warranty. In no event will the author be held liable for any damages
arising from the use of this software.

Permission to anyone to use this software for any purpose, including
commercial applications, and to alter it and redistribute it freely,
subject to the following restrictions:

1. The origin of this source code must not be misrepresented; you must not
   claim that you wrote the original source code. If you use this source code
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original source code.
3. This notice may not be removed or altered from any source distribution.

René Nyffenegger rene.nyffenegger@adp-gmbh.ch
*/

const ALPHABET: &[u8; 64] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/// Checks the canonical Base64 alphabet, padding, and unused trailing bits.
/// Padding may be omitted, matching Luanti's existing validator contract.
pub fn is_valid(bytes: &[u8]) -> bool {
    let data_len = bytes.iter().position(|&byte| byte == b'=').unwrap_or(bytes.len());
    let padding_len = bytes.len() - data_len;

    if padding_len > 2 || bytes[data_len..].iter().any(|&byte| byte != b'=') {
        return false;
    }

    let remainder = data_len % 4;
    if remainder == 1 {
        return false;
    }

    let expected_padding = match remainder {
        0 => 0,
        2 => 2,
        3 => 1,
        _ => unreachable!(),
    };

    if padding_len != 0 && padding_len != expected_padding {
        return false;
    }

    if bytes[..data_len]
        .iter()
        .any(|byte| !ALPHABET.contains(byte))
    {
        return false;
    }

    match remainder {
        2 => decode_digit(bytes[data_len - 1]).is_some_and(|digit| digit & 0x0f == 0),
        3 => decode_digit(bytes[data_len - 1]).is_some_and(|digit| digit & 0x03 == 0),
        _ => true,
    }
}

/// Encodes arbitrary bytes using the standard Base64 alphabet without `=` padding.
pub fn encode(input: &[u8]) -> String {
    let mut output = String::with_capacity(input.len().div_ceil(3) * 4);

    for chunk in input.chunks(3) {
        let first = chunk[0];
        let second = chunk.get(1).copied().unwrap_or(0);
        let third = chunk.get(2).copied().unwrap_or(0);
        let indices = [
            (first >> 2) as usize,
            (((first & 0x03) << 4) | (second >> 4)) as usize,
            (((second & 0x0f) << 2) | (third >> 6)) as usize,
            (third & 0x3f) as usize,
        ];
        let count = chunk.len() + 1;
        output.extend(indices[..count].iter().map(|&index| ALPHABET[index] as char));
    }

    output
}

/// Decodes valid standard Base64 text. Unpadded encodings are accepted.
pub fn decode(input: &[u8]) -> Result<Vec<u8>, DecodeError> {
    if !is_valid(input) {
        return Err(DecodeError);
    }

    let data_len = input.iter().position(|&byte| byte == b'=').unwrap_or(input.len());
    let data = &input[..data_len];
    let mut output = Vec::with_capacity(data.len() * 3 / 4);
    let mut accumulator = 0u32;
    let mut bits = 0u8;

    for &byte in data {
        accumulator = (accumulator << 6) | u32::from(decode_digit(byte).ok_or(DecodeError)?);
        bits += 6;
        if bits >= 8 {
            bits -= 8;
            output.push((accumulator >> bits) as u8);
        }
    }

    Ok(output)
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct DecodeError;

impl std::fmt::Display for DecodeError {
    fn fmt(&self, formatter: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        formatter.write_str("invalid Base64 input")
    }
}

impl std::error::Error for DecodeError {}

fn decode_digit(byte: u8) -> Option<u8> {
    ALPHABET.iter().position(|&candidate| candidate == byte).map(|index| index as u8)
}

#[cfg(test)]
mod tests {
    use super::{decode, encode, is_valid};

    #[test]
    fn encodes_binary_data_without_padding() {
        assert_eq!(encode(b""), "");
        assert_eq!(encode(b"f"), "Zg");
        assert_eq!(encode(b"fo"), "Zm8");
        assert_eq!(encode(b"foo"), "Zm9v");
        assert_eq!(encode(&[0, 255, 128]), "AP+A");
    }

    #[test]
    fn round_trips_arbitrary_binary_data() {
        for length in 0..=512 {
            let bytes: Vec<u8> = (0..length).map(|index| (index * 73) as u8).collect();
            assert_eq!(decode(encode(&bytes).as_bytes()).unwrap(), bytes);
        }
    }

    #[test]
    fn decodes_optional_padding() {
        assert_eq!(decode(b"Zg").unwrap(), b"f");
        assert_eq!(decode(b"Zg==").unwrap(), b"f");
        assert_eq!(decode(b"Zm8=").unwrap(), b"fo");
    }

    #[test]
    fn validates_canonical_forms_and_rejects_malformed_input() {
        for value in ["", "AA", "AAA", "AAAA", "AA==", "AAA=", "AQ", "AAE"] {
            assert!(is_valid(value.as_bytes()), "{value:?}");
        }
        for value in ["A", "A===", "AAAA====", "AB", "AB==", "A=A", "AA=A", "."] {
            assert!(!is_valid(value.as_bytes()), "{value:?}");
            assert!(decode(value.as_bytes()).is_err(), "{value:?}");
        }
        assert!(!is_valid(&[0xe1, b'A', b'A', b'A']));

    }
}
