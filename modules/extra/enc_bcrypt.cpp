/* RequiredLibraries: xcrypt */

#include "module.h"
#include "encryption.h"
#include <xcrypt.h>

class EBCRYPT : public Module
{
	unsigned int rounds;

	Anope::string Salt()
	{
		char entropy[16];
		for (unsigned int i = 0; i < sizeof(entropy); i++)
			entropy[i] = static_cast<char>(rand() % 0xFF);

		char salt[32];
		if (!crypt_gensalt_rn("$2a$", rounds, entropy, sizeof(entropy), salt, sizeof(salt)))
			return "";
		return salt;
	}

	Anope::string Generate(const Anope::string& data, const Anope::string& salt)
	{
		char hash[64];
		crypt_rn(data.c_str(), salt.c_str(), hash, sizeof(hash));
		return hash;
	}

	bool Compare(const Anope::string& string, const Anope::string& hash)
	{
		Anope::string ret = Generate(string, hash);
		if (ret.empty())
			return false;

		return (ret == hash);
	}

 public:
	EBCRYPT(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, ENCRYPTION | VENDOR | EXTRA),
		rounds(10)
	{
		// Should obviously never happen, but if xcrypt somehow was compiled without support for $2a$ I guess it might
		if (Salt() == "")
			throw ModuleException("Could not salt! BCrypt will not be available!");
	}

	EventReturn OnEncrypt(const Anope::string &src, Anope::string &dest) anope_override
	{
		dest = "bcrypt:" + Generate(src, Salt());
		Log(LOG_DEBUG_2) << "(enc_bcrypt) hashed password from [" << src << "] to [" << dest << "]";
		return EVENT_ALLOW;
	}

	void OnCheckAuthentication(User *, IdentifyRequest *req) anope_override
	{
		const NickAlias *na = NickAlias::Find(req->GetAccount());
		if (na == NULL)
			return;
		NickCore *nc = na->nc;

		size_t pos = nc->pass.find(':');
		if (pos == Anope::string::npos)
			return;
		Anope::string hash_method(nc->pass.begin(), nc->pass.begin() + pos);
		if (hash_method != "bcrypt")
			return;

		if (Compare(req->GetPassword(), nc->pass.substr(7)))
		{
			/* if we are NOT the first module in the list,
			 * we want to re-encrypt the pass with the new encryption
			 */

			unsigned int hashrounds = 0;
			try
			{
				size_t roundspos = nc->pass.find('$', 11);
				if (roundspos == Anope::string::npos)
					throw ConvertException("Could not find hashrounds");

				hashrounds = convertTo<unsigned int>(nc->pass.substr(11, roundspos - 11));
			}
			catch (const ConvertException &)
			{
				Log(this) << "Could not get the round size of a hash. This is probably a bug. Hash: " << nc->pass;
			}

			if (ModuleManager::FindFirstOf(ENCRYPTION) != this || (hashrounds && hashrounds != rounds))
				Anope::Encrypt(req->GetPassword(), nc->pass);
			req->Success(this);
		}
	}

	void OnReload(Configuration::Conf *conf) anope_override
	{
		Configuration::Block *block = conf->GetModule(this);
		rounds = block->Get<unsigned int>("rounds", "10");

		if (rounds == 0)
		{
			rounds = 10;
			Log(this) << "Rounds can't be 0! Setting ignored.";
		}
		else if (rounds < 10)
		{
			Log(this) << "10 to 12 rounds is recommended.";
		}
		else if (rounds >= 32)
		{
			rounds = 10;
			Log(this) << "The maximum number of rounds supported is 31. Ignoring setting and using 10.";
		}
		else if (rounds >= 14)
		{
			Log(this) << "Are you sure you want to use " << stringify(rounds) << " in your bcrypt settings? This is very CPU intensive! Recommended rounds is 10-12.";
		}
	}
};

MODULE_INIT(EBCRYPT)