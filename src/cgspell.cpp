/*
* Copyright (C) 2015-2018, Kevin Brubeck Unhammer <unhammer@fsfe.org>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "cgspell.hpp"

namespace divvun {

static const string subreading_separator = "#";
static const string unknown_analysis = " ?";

/**
 * Return the size in bytes of the first complete UTF-8 codepoint in c,
 * or 0 if invalid.
 */
size_t u8_first_codepoint_size(const unsigned char* c) {
    if (*c <= 127) {
        return 1;
    }
    else if ( (*c & (128 + 64 + 32 + 16)) == (128 + 64 + 32 + 16) ) {
        return 4;
    }
    else if ( (*c & (128 + 64 + 32 )) == (128 + 64 + 32) ) {
        return 3;
    }
    else if ( (*c & (128 + 64 )) == (128 + 64)) {
        return 2;
    }
    else {
        return 0;
    }
}

bool is_cg_tag(const string & str) {
    // Note: invalid codepoints are also treated as tags;  ¯\_(ツ)_/¯
    return str.size() > u8_first_codepoint_size((const unsigned char*)str.c_str());
}

void print_cg_subreading(size_t indent,
			 const string& form,
			 const vector<string>::const_iterator beg,
                         const vector<string>::const_iterator end,
                         std::ostream & os,
			 Weight w,
			 variant<Nothing, Weight> mw_a,
			 const std::string& errtag)
{
	os << string(indent, '\t');
	bool in_lemma = false;
	for(vector<string>::const_iterator it = beg; it != end; ++it) {
		bool is_tag = is_cg_tag(*it);
		if(in_lemma) {
			if(is_tag) {
				in_lemma = false;
				os << "\"";
			}
		}
		else {
			if(!is_tag) {
				in_lemma = true;
				os << "\"";
			}
		}
		os << (*it);
	}
	if(in_lemma) {
		os << "\"";
	}
	if(indent == 1) {
		os << " <W:" << w << ">";
		mw_a.match([]      (Nothing) {},
			   [&os] (Weight w_a) { os << " <WA:" << w_a << ">"; });
		os << " " << errtag;
		os << " \"<" << form << ">\"";
	}
	os << std::endl;
}

const void print_readings(const vector<string>& ana,
			  const string& form,
			  std::ostream& os,
			  Weight w,
			  variant<Nothing, Weight> w_a,
			  const std::string& errtag)
{
	size_t indent = 1;
	auto beg = ana.begin(), end = ana.end();
	while(true) {
		bool sub_found = false;
		for(auto it = end-1; it > ana.begin(); --it) {
			if(subreading_separator.compare(*it) == 0) {
				// Found a sub-reading mark
				beg = ++it;
				sub_found = true;
				break;
			}
		}
		if(!sub_found) {
			// No remaining sub-marks to the left
			beg = ana.begin();
		}
		print_cg_subreading(indent,
				    form,
				    beg,
				    end,
				    os,
				    w,
				    w_a,
				    errtag);
		if(beg == ana.begin()) {
			break;
		}
		else {
			++indent;
			end = beg;
			if(sub_found) {
				--end; // skip the subreading separator symbol
			}
		}
	}
}

void Speller::spell(const string& inform, std::ostream& os)
{
	bool do_suggest = real_word || !speller->spell(inform);
	if(!do_suggest) {
		if(analyse_when_correct) {
			// This would happen if a correct inform is in the
			// speller, but not in whatever analyser you used to
			// create the input to cgspell
			auto aq = speller->analyseSymbols(inform);
			while(!aq.empty()) {
				const auto ana = aq.top().first;
				const Weight& w = aq.top().second;
				// No max_weight for regular words
				print_readings(ana, inform, os, w, Nothing(), CGSPELL_CORRECT_TAG);
				aq.pop();
			}
		}
	}
	else if (cache.find(inform) != cache.end()) {
		os << cache[inform];
	}
	else {
		auto cq = speller->suggest(inform);
		auto slimit = limit;
		std::ostringstream result;
		while(!cq.empty() && (slimit--) > 0) {
			const auto& corrform = cq.top().first;
			const Weight& w = cq.top().second;
			if(inform == "bidjojuvvojit") {
				std::cerr << slimit<<"\t"<<corrform<<":"<<w << std::endl;
			}
			if(max_weight > 0.0 && w >= max_weight) {
				break;
			}
			auto aq = speller->analyseSymbols(corrform, true);
			while(!aq.empty()) {
				const auto& ana = aq.top().first;
				const Weight& w_a = (aq.top().second);
				if(max_analysis_weight > 0.0 && w_a >= max_analysis_weight) {
					break;
				}
				print_readings(ana, corrform, result, w, w_a, CGSPELL_TAG);
				aq.pop();
			}
			cq.pop();
		}
		if(cache.size() > cache_max) {
			std::unordered_map<string, string>().swap(cache);
		}
		cache[inform] = result.str();
		os << result.str();
	}
}


void proc_sent(const SpellSent& sent, std::ostream& os, Speller& s) {
	bool do_spell = (sent.cohorts.size() < s.min_sent_max_unknown)
		|| (sent.n_unknowns <= s.max_sent_unknown_rate * sent.cohorts.size());
	for(const auto& r : sent.cohorts) {
		for(const auto& line : r.lines) {
			os << line << std::endl;
		}
		if (!r.wf.empty() && (s.real_word || r.unknown))
		{
			if(do_spell) {
				s.spell(r.wf, os);
			}
			else {
				os << "\t\"" << r.wf << "\" ? <spellskip>" << std::endl;
			}
		}
		for(const auto& postblank : r.postblank) {
			os << postblank << std::endl;
		}
	}
}

void run_cgspell(std::istream& is,
		 std::ostream& os,
		 Speller& s)
{
	SpellSent sent = { {}, 0 };
	SpellCohort c = { "", {}, {}, false };
	for (string line; std::getline(is, line);) {
		std::match_results<const char*> result;
		std::regex_match(line.c_str(), result, CG_LINE);
		if (!result.empty() && result[2].length() != 0) {
			sent.cohorts.push_back(c);
			// Was the previous cohort a sent delimiter?
			std::match_results<const char*> del_res;
			std::regex_match(c.wf.c_str(), del_res, s.sent_delimiters);
			if(!del_res.empty() && del_res[0].length() != 0) {
				proc_sent(sent, os, s);
				sent = { {}, 0 };
			}
			c = SpellCohort({ result[2], {}, {}, false});
			c.lines.push_back(line);
		}
		else if (!result.empty() && result[5].length() != 0)
		{
			c.unknown = (result[5] == unknown_analysis);
			if(c.unknown) {
				sent.n_unknowns += 1;
			}
			c.lines.push_back(line);
		}
		else if(!result.empty() && result[7].length() != 0) {
			// TODO: Can we ever get a flush in the middle of readings?
			sent.cohorts.push_back(c);
			proc_sent(sent, os, s);
			sent = { {}, 0 };
			c = SpellCohort({ "", {}, {}, false });
			std::cout << line << std::endl;
			os.flush();
		}
		else {
			c.postblank.push_back(line);
		}
	}
	sent.cohorts.push_back(c);
	proc_sent(sent, os, s);
}

}
