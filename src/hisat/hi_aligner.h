/*
 * Copyright 2014, Daehwan Kim <infphilo@gmail.com>
 *
 * This file is part of HISAT.
 *
 * HISAT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HISAT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HISAT.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HI_ALIGNER_H_
#define HI_ALIGNER_H_

#include <iostream>
#include <utility>
#include <limits>
#include "qual.h"
#include "ds.h"
#include "sstring.h"
#include "alphabet.h"
#include "edit.h"
#include "read.h"
// Threading is necessary to synchronize the classes that dump
// intermediate alignment results to files.  Otherwise, all data herein
// is constant and shared, or per-thread.
#include "threading.h"
#include "aligner_result.h"
#include "aligner_cache.h"
#include "scoring.h"
#include "mem_ids.h"
#include "simple_func.h"
#include "aligner_driver.h"
#include "aligner_sw_driver.h"
#include "group_walk.h"

// Maximum insertion length
static const uint32_t maxInsLen = 3;
// Maximum deletion length
static const uint32_t maxDelLen = 3;

// Minimum anchor length required for canonical splice sites
static const uint32_t minAnchorLen = 7;
// Minimum anchor length required for non-canonical splice sites
static const uint32_t minAnchorLen_noncan = 14;

// Allow longer introns for long anchored reads involving canonical splice sites
inline uint32_t MaxIntronLen(uint32_t anchor) {
    uint32_t intronLen = 0;
    if(anchor >= minAnchorLen) {
        assert_geq(anchor, 2);
        uint32_t shift = (anchor << 1) - 4;
        shift = min<uint32_t>(max<uint32_t>(shift, 13), 30);
        intronLen = 1 << shift;
    }
    return intronLen;
}

inline float intronLen_prob(uint32_t anchor, uint32_t intronLen, uint32_t maxIntronLen) {
    uint32_t expected_intron_len = maxIntronLen;
    if(anchor < 14) expected_intron_len = 1 << ((anchor << 1) + 4);
    if(expected_intron_len > maxIntronLen) expected_intron_len = maxIntronLen;
    assert_gt(expected_intron_len, 0);
    float result = ((float)intronLen) / ((float)expected_intron_len);
    if(result > 1.0f) result = 1.0f;
    return result;
}

// Allow longer introns for long anchored reads involving non-canonical splice sites
inline uint32_t MaxIntronLen_noncan(uint32_t anchor) {
    uint32_t intronLen = 0;
    if(anchor >= minAnchorLen_noncan) {
        assert_geq(anchor, 5);
        uint32_t shift = (anchor << 1) - 10;
        shift = min<uint32_t>(shift, 30);
        intronLen = 1 << shift;
    }
    return intronLen;
}

inline float intronLen_prob_noncan(uint32_t anchor, uint32_t intronLen, uint32_t maxIntronLen) {
    uint32_t expected_intron_len = maxIntronLen;
    if(anchor < 16) expected_intron_len = 1 << (anchor << 1);
    if(expected_intron_len > maxIntronLen) expected_intron_len = maxIntronLen;
    assert_gt(expected_intron_len, 0);
    float result = ((float)intronLen) / ((float)expected_intron_len);
    if(result > 1.0f) result = 1.0f;
    return result;
}

/**
 * Hit types for BWTHit class below
 * Three hit types to anchor a read on the genome
 *
 */
enum {
    CANDIDATE_HIT = 1,
    PSEUDOGENE_HIT,
    ANCHOR_HIT,
};

/**
 * Simple struct for holding a partial alignment for the read
 * The alignment locations are represented by FM offsets [top, bot),
 * and later genomic offsets are calculated when necessary
 */
template <typename index_t>
struct BWTHit {
	
	BWTHit() { reset(); }
	
	void reset() {
		_top = _bot = 0;
		_fw = true;
		_bwoff = (index_t)OFF_MASK;
		_len = 0;
		_coords.clear();
        _anchor_examined = false;
        _hit_type = CANDIDATE_HIT;
	}
	
	void init(
			  index_t top,
			  index_t bot,
  			  bool fw,
			  uint32_t bwoff,
			  uint32_t len,
              index_t hit_type = CANDIDATE_HIT)
	{
		_top = top;
        _bot = bot;
		_fw = fw;
		_bwoff = bwoff;
		_len = len;
        _coords.clear();
        _anchor_examined = false;
        _hit_type = hit_type;
	}
    
    bool hasGenomeCoords() const { return !_coords.empty(); }
	
	/**
	 * Return true iff there is no hit.
	 */
	bool empty() const {
		return _bot <= _top;
	}
	
	/**
	 * Higher score = higher priority.
	 */
	bool operator<(const BWTHit& o) const {
		return _len > o._len;
	}
	
	/**
	 * Return the size of the alignments SA ranges.
	 */
	index_t size() const {
        assert_leq(_top, _bot);
        return _bot - _top;
    }
    
    index_t len() const {
        assert_gt(_len, 0);
        return _len;
    }
	
#ifndef NDEBUG
	/**
	 * Check that hit is sane w/r/t read.
	 */
	bool repOk(const Read& rd) const {
		assert_gt(_bot, _top);
		assert_neq(_bwoff, (index_t)OFF_MASK);
		assert_gt(_len, 0);
		return true;
	}
#endif
	
	index_t         _top;               // start of the range in the FM index
	index_t         _bot;               // end of the range in the FM index
	bool            _fw;                // whether read is forward or reverse complemented
	index_t         _bwoff;             // current base of a read to search from the right end
	index_t         _len;               // read length
	
    EList<Coord>    _coords;            // genomic offsets corresponding to [_top, _bot)
    
    bool            _anchor_examined;   // whether or not this hit is examined
    index_t         _hit_type;          // hit type (anchor hit, pseudogene hit, or candidate hit)
};


/**
 * Simple struct for holding alignments for the read
 * The alignments are represented by chains of BWTHits
 */
template <typename index_t>
struct ReadBWTHit {
	
	ReadBWTHit() { reset(); }
	
	void reset() {
        _fw = true;
		_len = 0;
        _cur = 0;
        _done = false;
        _numPartialSearch = 0;
        _numUniqueSearch = 0;
        _partialHits.clear();
	}

	void init(
			  bool fw,
              index_t len)
	{
        _fw = fw;
        assert_gt(len, 0);
        _len = len;
        _cur = 0;
        _done = false;
        _numPartialSearch = 0;
        _numUniqueSearch = 0;
        _partialHits.clear();
	}
    
    bool done() {
#ifndef NDEBUG
        assert_gt(_len, 0);
        if(_cur >= _len) {
            assert(_done);
        }
#endif
        return _done;
    }
    
    void done(bool done) {
        assert(!_done);
        assert(done);
        _done = done;
    }
    
    index_t len() const { return _len; }
    index_t cur() const { return _cur; }
    
    size_t  offsetSize()             { return _partialHits.size(); }
    size_t  numPartialSearch()       { return _numPartialSearch; }
    size_t  numActualPartialSearch()
    {
        assert_leq(_numUniqueSearch, _numPartialSearch);
        return _numPartialSearch - _numUniqueSearch;
    }
    
    bool width(index_t offset_) {
        assert_lt(offset_, _partialHits.size());
        return _partialHits[offset_].size();
    }
    
    bool hasGenomeCoords(index_t offset_) {
        assert_lt(offset_, _partialHits.size());
        index_t width_ = width(offset_);
        if(width_ == 0) {
            return true;
        } else {
            return _partialHits[offset_].hasGenomeCoords();
        }
    }
    
    bool hasAllGenomeCoords() {
        if(_cur < _len) return false;
        if(_partialHits.size() <= 0) return false;
        for(size_t oi = 0; oi < _partialHits.size(); oi++) {
            if(!_partialHits[oi].hasGenomeCoords())
                return false;
        }
        return true;
    }
    
    /**
     *
     */
    index_t minWidth(index_t& offset) const {
        index_t minWidth_ = (index_t)OFF_MASK;
        index_t minWidthLen_ = 0;
        for(size_t oi = 0; oi < _partialHits.size(); oi++) {
            const BWTHit<index_t>& hit = _partialHits[oi];
            if(hit.empty()) continue;
            // if(!hit.hasGenomeCoords()) continue;
            assert_gt(hit.size(), 0);
            if((minWidth_ > hit.size()) ||
               (minWidth_ == hit.size() && minWidthLen_ < hit.len())) {
                minWidth_ = hit.size();
                minWidthLen_ = hit.len();
                offset = (index_t)oi;
            }
        }
        return minWidth_;
    }
    
    // add policy for calculating a search score
    int64_t searchScore(index_t minK) {
        int64_t score = 0;
        const int64_t penaltyPerOffset = minK * minK;
        for(size_t i = 0; i < _partialHits.size(); i++) {
            index_t len = _partialHits[i]._len;
            score += (len * len);
        }
        
        assert_geq(_numPartialSearch, _partialHits.size());
        index_t actualPartialSearch = numActualPartialSearch();
        score -= (actualPartialSearch * penaltyPerOffset);
        score -= (1 << (actualPartialSearch << 1));
        return score;
    }
    
    BWTHit<index_t>& getPartialHit(index_t offset_) {
        assert_lt(offset_, _partialHits.size());
        return _partialHits[offset_];
    }
    
    bool adjustOffset(index_t minK) {
        assert_gt(_partialHits.size(), 0);
        const BWTHit<index_t>& hit = _partialHits.back();
        if(hit.len() >= minK + 3) {
            return false;
        }
        assert_geq(_cur, hit.len());
        index_t origCur = _cur - hit.len();
        _cur = origCur + max(hit.len(), minK + 1) - minK;
        _partialHits.pop_back();
        return true;
    }
    
    void setOffset(index_t offset) {
        assert_lt(offset, _len);
        _cur = offset;
    }
    
#ifndef NDEBUG
	/**
	 */
	bool repOk() const {
        for(size_t i = 0; i < _partialHits.size(); i++) {
            if(i == 0) {
                assert_geq(_partialHits[i]._bwoff, 0);
            }
            
            if(i + 1 < _partialHits.size()) {
                assert_leq(_partialHits[i]._bwoff + _partialHits[i]._len, _partialHits[i+1]._bwoff);
            } else {
                assert_eq(i+1, _partialHits.size());
                assert_eq(_partialHits[i]._bwoff + _partialHits[i]._len, _cur);
            }
        }
		return true;
	}
#endif
	
	bool     _fw;
	index_t  _len;
    index_t  _cur;
    bool     _done;
    index_t  _numPartialSearch;
    index_t  _numUniqueSearch;
    index_t  _cur_local;
    
    EList<BWTHit<index_t> >       _partialHits;
};


/**
 * this is per-thread data, which are shared by GenomeHit classes
 * the main purpose of this struct is to avoid extensive use of memory related functions
 * such as new and delete - those are really slow and lock based
 */
template <typename index_t>
struct SharedTempVars {
    SStringExpandable<char> raw_refbuf;
    SStringExpandable<char> raw_refbuf2;
    EList<int64_t> temp_scores;
    EList<int64_t> temp_scores2;
    ASSERT_ONLY(SStringExpandable<uint32_t> destU32);
    
    ASSERT_ONLY(BTDnaString editstr);
    ASSERT_ONLY(BTDnaString partialseq);
    ASSERT_ONLY(BTDnaString refstr);
    ASSERT_ONLY(EList<index_t> reflens);
    ASSERT_ONLY(EList<index_t> refoffs);
    
    LinkedEList<EList<Edit> > raw_edits;
};

/**
 * GenomeHit represents read alignment or alignment of a part of a read
 * Two GenomeHits that represents alignments of different parts of a read
 * can be combined together.  Also, GenomeHit can be extended in both directions.
 */
template <typename index_t>
struct GenomeHit {
	
	GenomeHit() :
    _fw(false),
    _rdoff((index_t)OFF_MASK),
    _len((index_t)OFF_MASK),
    _trim5(0),
    _trim3(0),
    _tidx((index_t)OFF_MASK),
    _toff((index_t)OFF_MASK),
    _edits(NULL),
    _score(MIN_I64),
    _hitcount(1),
    _edits_node(NULL),
    _sharedVars(NULL)
    {
    }
    
    GenomeHit(const GenomeHit& otherHit) :
    _fw(false),
    _rdoff((index_t)OFF_MASK),
    _len((index_t)OFF_MASK),
    _trim5(0),
    _trim3(0),
    _tidx((index_t)OFF_MASK),
    _toff((index_t)OFF_MASK),
    _edits(NULL),
    _score(MIN_I64),
    _hitcount(1),
    _edits_node(NULL),
    _sharedVars(NULL)
    {
        init(otherHit._fw,
             otherHit._rdoff,
             otherHit._len,
             otherHit._trim5,
             otherHit._trim3,
             otherHit._tidx,
             otherHit._toff,
             *(otherHit._sharedVars),
             otherHit._edits,
             otherHit._score,
             otherHit._splicescore);
    }
    
    GenomeHit<index_t>& operator=(const GenomeHit<index_t>& otherHit) {
        if(this == &otherHit) return *this;
        init(otherHit._fw,
             otherHit._rdoff,
             otherHit._len,
             otherHit._trim5,
             otherHit._trim3,
             otherHit._tidx,
             otherHit._toff,
             *(otherHit._sharedVars),
             otherHit._edits,
             otherHit._score,
             otherHit._splicescore);
        
        return *this;
    }
    
    ~GenomeHit() {
        if(_edits_node != NULL) {
            assert(_edits != NULL);
            assert(_sharedVars != NULL);
            _sharedVars->raw_edits.delete_node(_edits_node);
            _edits = NULL;
            _edits_node = NULL;
            _sharedVars = NULL;
        }
    }
	
	void init(
              bool                      fw,
			  index_t                   rdoff,
			  index_t                   len,
              index_t                   trim5,
              index_t                   trim3,
              index_t                   tidx,
              index_t                   toff,
              SharedTempVars<index_t>&  sharedVars,
              EList<Edit>*              edits = NULL,
              int64_t                   score = 0,
              double                    splicescore = 0.0)
	{
		_fw = fw;
		_rdoff = rdoff;
		_len = len;
        _trim5 = trim5;
        _trim3 = trim3;
        _tidx = tidx;
        _toff = toff;
		_score = score;
        _splicescore = splicescore;
        
        assert(_sharedVars == NULL || _sharedVars == &sharedVars);
        _sharedVars = &sharedVars;
        if(_edits == NULL) {
            assert(_edits_node == NULL);
            _edits_node = _sharedVars->raw_edits.new_node();
            assert(_edits_node != NULL);
            _edits = &(_edits_node->payload);
        }
        assert(_edits != NULL);
        _edits->clear();
        
        if(edits != NULL) *_edits = *edits;
        _hitcount = 1;
	}
    
    bool inited() const {
        return _len >= 0 && _len < (index_t)OFF_MASK;
    }
    
    /**
     * Check if it is compatible with another GenomeHit with respect to indels or introns
     */
    bool compatibleWith(
                        const GenomeHit<index_t>& otherHit,
                        index_t minIntronLen,
                        index_t maxIntronLen,
                        bool no_spliced_alignment = false) const;
    
    /**
     * Combine itself with another GenomeHit
     */
    bool combineWith(
                     const GenomeHit&           otherHit,
                     const Read&                rd,
                     const BitPairReference&    ref,
                     SpliceSiteDB&              ssdb,
                     SwAligner&                 swa,
                     SwMetrics&                 swm,
                     const Scoring&             sc,
                     TAlScore                   minsc,
                     RandomSource&              rnd,           // pseudo-random source
                     index_t                    minK_local,
                     index_t                    minIntronLen,
                     index_t                    maxIntronLen,
                     index_t                    can_mal = minAnchorLen,           // minimum anchor length for canonical splice site
                     index_t                    noncan_mal = minAnchorLen_noncan,       // minimum anchor length for non-canonical splice site
                     const SpliceSite*          spliceSite = NULL,    // penalty for splice site
                     bool                       no_spliced_alignment = false);
    
    /**
     * Extend the partial alignment (GenomeHit) bidirectionally
     */
    bool extend(
                const Read&             rd,
                const BitPairReference& ref,
                SpliceSiteDB&           ssdb,
                SwAligner&              swa,
                SwMetrics&              swm,
                PerReadMetrics&         prm,
                const Scoring&          sc,
                TAlScore                minsc,
                RandomSource&           rnd,           // pseudo-random source
                index_t                 minK_local,
                index_t                 minIntronLen,
                index_t                 maxIntronLen,
                index_t&                leftext,
                index_t&                rightext,
                index_t                 mm = 0);
    
    /**
     * For alignment involving indel, move the indels
     * to the left most possible position
     */
    void leftAlign(const Read& rd);
    
    index_t rdoff() const { return _rdoff; }
    index_t len()   const { return _len; }
    index_t trim5() const { return _trim5; }
    index_t trim3() const { return _trim3; }
    
    void trim5(index_t trim5) { _trim5 = trim5; }
    void trim3(index_t trim3) { _trim3 = trim3; }
    
    index_t ref()    const { return _tidx; }
    index_t refoff() const { return _toff; }
    index_t fw()     const { return _fw; }
    
    index_t hitcount() const { return _hitcount; }
    
    /**
     * Leftmost coordinate
     */
    Coord coord() const {
        return Coord(_tidx, _toff, _fw);
    }
    
    int64_t score() const { return _score; }
    double  splicescore() const { return _splicescore; }
    
    const EList<Edit>& edits() const { return *_edits; }
    
    /**
     * Retrieve the partial alignment from the left until indel or intron
     */
    void getLeft(index_t&       rdoff,
                 index_t&       len,
                 index_t&       toff,
                 int64_t*       score = NULL,
                 const Read*    rd = NULL,
                 const Scoring* sc = NULL) const
    {
        assert(inited());
        toff = _toff, rdoff = _rdoff, len = _len;
        const BTString* qual = NULL;
        if(score != NULL) {
            assert(rd != NULL);
            assert(sc != NULL);
            *score = 0;
            qual = &(_fw ? rd->qual : rd->qualRev);
        }
        for(index_t i = 0; i < _edits->size(); i++) {
            const Edit& edit = (*_edits)[i];
            if(edit.type == EDIT_TYPE_SPL ||
               edit.type == EDIT_TYPE_READ_GAP ||
               edit.type == EDIT_TYPE_REF_GAP) {
                len = edit.pos;
                break;
            }
            if(score != NULL) {
                if(edit.type == EDIT_TYPE_MM) {
                    assert(qual != NULL);
                    *score += sc->score(
                                        dna2col[edit.qchr] - '0',
                                        asc2dnamask[edit.chr],
                                        (*qual)[this->_rdoff + edit.pos] - 33);
                }
            }
        }
        assert_geq(len, 0);
    }
    
    /**
     * Retrieve the partial alignment from the right until indel or intron
     */
    void getRight(index_t&       rdoff,
                  index_t&       len,
                  index_t&       toff,
                  int64_t*       score = NULL,
                  const Read*    rd = NULL,
                  const Scoring* sc = NULL) const
    {
        assert(inited());
        toff = _toff, rdoff = _rdoff, len = _len;
        const BTString* qual = NULL;
        if(score != NULL) {
            assert(rd != NULL);
            assert(sc != NULL);
            *score = 0;
            qual = &(_fw ? rd->qual  : rd->qualRev);
        }
        if(_edits->size() == 0) return;
        for(int i = _edits->size() - 1; i >= 0; i--) {
            const Edit& edit = (*_edits)[i];
            if(edit.type == EDIT_TYPE_SPL ||
               edit.type == EDIT_TYPE_READ_GAP ||
               edit.type == EDIT_TYPE_REF_GAP) {
                rdoff = _rdoff + edit.pos;
                assert_lt(edit.pos, _len);
                len = _len - edit.pos;
                if(edit.type == EDIT_TYPE_REF_GAP) {
                    assert_lt(edit.pos + 1, _len);
                    assert_gt(len, 1);
                    rdoff++;
                    len--;
                }
                toff = getRightOff() - len;
                break;
            }
            if(score != NULL) {
                if(edit.type == EDIT_TYPE_MM) {
                    assert(qual != NULL);
                    *score += sc->score(
                                        dna2col[edit.qchr] - '0',
                                        asc2dnamask[edit.chr],
                                        (*qual)[this->_rdoff + edit.pos] - 33);
                }
            }
        }
        assert_gt(len, 0);
    }
    
    /**
     * Retrieve the genomic offset of the right end
     */
    index_t getRightOff() const {
        assert(inited());
        index_t toff = _toff + _len;
        for(index_t i = 0; i < _edits->size(); i++) {
            const Edit& ed = (*_edits)[i];
            if(ed.type == EDIT_TYPE_SPL) {
                toff += ed.splLen;
            } else if(ed.type == EDIT_TYPE_READ_GAP) {
                toff++;
            } else if(ed.type == EDIT_TYPE_REF_GAP) {
                assert_gt(toff, 0);
                toff--;
            }
        }
        return toff;
    }
    
    /**
     * Retrieve left anchor length and number of edits in the anchor
     */
    void getLeftAnchor(index_t& leftanchor,
                       index_t& nedits) const
    {
        assert(inited());
        leftanchor = _len;
        nedits = 0;
        for(index_t i = 0; i < _edits->size(); i++) {
            const Edit& edit = (*_edits)[i];
            if(edit.type == EDIT_TYPE_SPL) {
                leftanchor = edit.pos;
                break;
            } else if(edit.type == EDIT_TYPE_MM ||
                      edit.type == EDIT_TYPE_READ_GAP ||
                      edit.type == EDIT_TYPE_REF_GAP) {
                nedits++;
            }
        }
    }
    
    /**
     * Retrieve right anchor length and number of edits in the anchor
     */
    void getRightAnchor(index_t& rightanchor,
                        index_t& nedits) const
    {
        rightanchor = _len;
        nedits = 0;
        if(_edits->size() == 0) return;
        for(int i = _edits->size() - 1; i >= 0; i--) {
            const Edit& edit = (*_edits)[i];
            if(edit.type == EDIT_TYPE_SPL) {
                rightanchor = _len - edit.pos - 1;
                break;
            } else if(edit.type == EDIT_TYPE_MM ||
                      edit.type == EDIT_TYPE_READ_GAP ||
                      edit.type == EDIT_TYPE_REF_GAP) {
                nedits++;
            }
        }
    }
    
    
    /**
     * Is it spliced alignment?
     */
    bool spliced() const {
        for(index_t i = 0; i < _edits->size(); i++) {
            if((*_edits)[i].type == EDIT_TYPE_SPL) {
                return true;
            }
        }
        return false;
    }
    
    /**
     *
     */
    bool spliced_consistently() const {
        uint32_t splDir = EDIT_SPL_UNKNOWN;
        for(index_t i = 0; i < _edits->size(); i++) {
            const Edit& edit = (*_edits)[i];
            if(edit.type == EDIT_TYPE_SPL) {
                if(splDir != EDIT_SPL_UNKNOWN) {
                    if(edit.splDir != EDIT_SPL_UNKNOWN) {
                        if(splDir != edit.splDir)
                            return false;
                    }
                } else {
                    splDir = _edits[i].splDir;
                }
            }
        }
        return true;
    }
    
    bool operator== (const GenomeHit<index_t>& other) const {
        if(_fw != other._fw ||
           _rdoff != other._rdoff ||
           _len != other._len ||
           _tidx != other._tidx ||
           _toff != other._toff ||
           _trim5 != other._trim5 ||
           _trim3 != other._trim3) {
            return false;
        }
        
        if(_edits->size() != other._edits->size()) return false;
        for(index_t i = 0; i < _edits->size(); i++) {
            if(!((*_edits)[i] == (*other._edits)[i])) return false;
        }
        // daehwan - this may not be true when some splice sites are provided from outside
        // assert_eq(_score, other._score);
        return true;
    }
    
    bool contains(const GenomeHit<index_t>& other) const {
        return (*this) == other;
    }

	/**
	 * Return number of mismatches in the alignment.
	 */
	int mms() const {
#if 0
		if     (_e2.inited()) return 2;
		else if(_e1.inited()) return 1;
		else                  return 0;
#endif
        return 0;
	}
	
	/**
	 * Return the number of Ns involved in the alignment.
	 */
	int ns() const {
#if 0
		int ns = 0;
		if(_e1.inited() && _e1.hasN()) {
			ns++;
			if(_e2.inited() && _e2.hasN()) {
				ns++;
			}
		}
		return ns;
#endif
        return 0;
	}
    
    int ngaps() const {
        return 0;
    }

#if 0
	/**
	 * Return the number of Ns involved in the alignment.
	 */
	int refns() const {
		int ns = 0;
		if(_e1.inited() && _e1.chr == 'N') {
			ns++;
			if(_e2.inited() && _e2.chr == 'N') {
				ns++;
			}
		}
		return ns;
	}

	/**
	 * Higher score = higher priority.
	 */
	bool operator<(const GenomeHit& o) const {
		return _len > o._len;
	}
#endif
	
#ifndef NDEBUG
	/**
	 * Check that hit is sane w/r/t read.
	 */
	bool repOk(const Read& rd, const BitPairReference& ref);
#endif
    
private:
    /**
	 * Calculate alignment score
	 */
    int64_t calculateScore(
                           const Read&      rd,
                           SpliceSiteDB&    ssdb,
                           const Scoring&   sc,
                           index_t          minK_local,
                           index_t          minIntronLen,
                           index_t          maxIntronLen,
                           const            BitPairReference& ref);
    
public:
	bool            _fw;
	index_t         _rdoff;
	index_t         _len;
    index_t         _trim5;
    index_t         _trim3;
    
    index_t         _tidx;
    index_t         _toff;
	EList<Edit>*    _edits;
    int64_t         _score;
    double          _splicescore;
    
    index_t         _hitcount;  // for selection purposes
    
    LinkedEListNode<EList<Edit> >*  _edits_node;
    SharedTempVars<index_t>* _sharedVars;
};

/**
 * Check if it is compatible with another GenomeHit with respect to indels or introns
 */
template <typename index_t>
bool GenomeHit<index_t>::compatibleWith(
                                        const GenomeHit<index_t>& otherHit,
                                        index_t minIntronLen,
                                        index_t maxIntronLen,
                                        bool no_spliced_alignment) const
{
    if(this == &otherHit) return false;
    // check if they are on the same strand and on the same contig
    if(_fw != otherHit._fw || _tidx != otherHit._tidx) return false;
    // make sure itself is closer to the left end of read than otherHit
    if(_rdoff > otherHit._rdoff) return false;
    // do not consider a case itself (read portion) includes otherHit
    if(_rdoff + _len > otherHit._rdoff + otherHit._len) return false;
    // make sure itself comes before otherHit wrt. genomic positions
    if(_toff > otherHit._toff) return false;
    
    index_t this_rdoff, this_len, this_toff;
    this->getRight(this_rdoff, this_len, this_toff);
    assert_geq(this_len, 0);
    index_t other_rdoff, other_len, other_toff;
    otherHit.getLeft(other_rdoff, other_len, other_toff);
    assert_geq(other_len, 0);
    
    if(this_rdoff > other_rdoff) return false;
    if(this_rdoff + this_len > other_rdoff + other_len) return false;
    if(this_toff > other_toff) return false;
    
    index_t refdif = other_toff - this_toff;
    index_t rddif = other_rdoff - this_rdoff;
    
    // check if there is a deletion, an insertion, or a potential intron
    // between the two partial alignments
    if(rddif != refdif) {
        if(rddif > refdif) {
            if(rddif > refdif + maxInsLen) return false;
        } else {
            assert_geq(refdif, rddif);
            if(refdif - rddif < minIntronLen) {
                if(refdif - rddif > maxDelLen) return false;
            } else {
                if(no_spliced_alignment) return false;
                if(refdif - rddif > maxIntronLen) {
                    return false;
                }
            }
        }
    }
    return true;
}

/**
 * Combine itself with another GenomeHit
 * while allowing mismatches, an insertion, a deletion, or an intron
 */
template <typename index_t>
bool GenomeHit<index_t>::combineWith(
                                     const GenomeHit&           otherHit,
                                     const Read&                rd,
                                     const BitPairReference&    ref,
                                     SpliceSiteDB&              ssdb,
                                     SwAligner&                 swa,
                                     SwMetrics&                 swm,
                                     const Scoring&             sc,
                                     TAlScore                   minsc,
                                     RandomSource&              rnd,           // pseudo-random source
                                     index_t                    minK_local,
                                     index_t                    minIntronLen,
                                     index_t                    maxIntronLen,
                                     index_t                    can_mal,       // minimum anchor length for canonical splice site
                                     index_t                    noncan_mal,    // minimum anchor length for non-canonical splice site
                                     const SpliceSite*          spliceSite,    // penalty for splice site
                                     bool                       no_spliced_alignment)
{
    if(this == &otherHit) return false;
    assert(compatibleWith(otherHit, minIntronLen, maxIntronLen, no_spliced_alignment));
    assert_eq(this->_tidx, otherHit._tidx);
    assert_lt(this->_tidx, ref.numRefs());
    
    // get the partial part of the alignment from the right
    // until an indel or splice sites
    index_t this_rdoff, this_len, this_toff;
    int64_t this_score;
    this->getRight(this_rdoff, this_len, this_toff, &this_score, &rd, &sc);
    assert_geq(this_len, 0);
    assert_leq(this_score, 0);
    assert_geq(this_score, this->_score);
    
    // get the partial part of the other alignment from the left
    // until an indel or splice sites
    index_t other_rdoff, other_len, other_toff;
    int64_t other_score;
    otherHit.getLeft(other_rdoff, other_len, other_toff, &other_score, &rd, &sc);
    assert_geq(other_len, 0);
    assert_leq(other_score, 0);
    assert_geq(other_score, otherHit._score);
    
    assert_leq(this_rdoff, other_rdoff);
    if(this_len != 0 &&
       other_len != 0 &&
       this_rdoff + this_len >= other_rdoff + other_len) return false;
    assert_leq(this_rdoff + this_len, other_rdoff + other_len);
    index_t len = other_rdoff - this_rdoff + other_len;
    const index_t reflen = ref.approxLen(_tidx);
    if(this_toff + len > reflen) return false;
    assert_leq(this_toff + len, reflen);
    assert_geq(other_toff + other_len, len);
    
    // check if an indel or an intron is necessary
    index_t refdif = other_toff - this_toff;
    index_t rddif = other_rdoff - this_rdoff;
    bool spliced = false, ins = false, del = false;
    if(refdif != rddif) {
        if(refdif > rddif) {
            if(refdif - rddif >= minIntronLen) {
                assert_leq(refdif - rddif, maxIntronLen);
                spliced = true;
            } else {
                assert_leq(refdif - rddif, maxDelLen);
                del = true;
            }
        } else {
            assert_leq(rddif - refdif, maxInsLen);
            ins = true;
        }
    }
#ifndef NDEBUG
    if(ins) {
        assert(!spliced && !del);
    } else {
        if(spliced) assert(!del);
        else        assert(!spliced);
    }
#endif
    
    if(no_spliced_alignment) {
        if(spliced) return false;
    }
    
    // if the combination of the two alignments does not involve an indel or an intron,
    // then simply combine them and return
    if(!spliced && !ins && !del && this_rdoff + this_len == other_rdoff) {
        index_t addoff = otherHit._rdoff - this->_rdoff;
        for(index_t i = 0; i < otherHit._edits->size(); i++) {
            _edits->push_back((*otherHit._edits)[i]);
            _edits->back().pos += addoff;
        }
        _len += otherHit._len;
        _score = calculateScore(
                                rd,
                                ssdb,
                                sc,
                                minK_local,
                                minIntronLen,
                                maxIntronLen,
                                ref);
        assert(repOk(rd, ref));
        return true;
    }
   
    // calculate the maximum gap lengths based on the current score and the mimumimu alignment score to be reported
    const BTDnaString& seq = this->_fw ? rd.patFw : rd.patRc;
    const BTString& qual = this->_fw ? rd.qual : rd.qualRev;
    index_t rdlen = seq.length();
    int64_t remainsc = minsc - (_score - this_score) - (otherHit._score - other_score);
    if(remainsc > 0) remainsc = 0;
    int read_gaps = 0;
    ASSERT_ONLY(int ref_gaps = 0);
    if(spliced) {
        read_gaps = sc.maxReadGaps(remainsc + sc.canSpl(), rdlen);
        ASSERT_ONLY(ref_gaps = sc.maxRefGaps(remainsc + sc.canSpl(), rdlen));
    }
    int this_ref_ext = read_gaps;
    if(spliced) this_ref_ext += (int)intronic_len;
    if(this_toff + len > reflen) return false;
    if(this_toff + len + this_ref_ext > reflen) this_ref_ext = reflen - (this_toff + len);
    assert(_sharedVars != NULL);
    SStringExpandable<char>& raw_refbuf = _sharedVars->raw_refbuf;
    EList<int64_t>& temp_scores = _sharedVars->temp_scores;
    EList<int64_t>& temp_scores2 = _sharedVars->temp_scores2;
    ASSERT_ONLY(SStringExpandable<uint32_t>& destU32 = _sharedVars->destU32);
    raw_refbuf.resize(len + this_ref_ext + 16);
    int off = ref.getStretch(
                             reinterpret_cast<uint32_t*>(raw_refbuf.wbuf()),
                             (size_t)this->_tidx,
                             (size_t)this_toff,
                             len + this_ref_ext
                             ASSERT_ONLY(, destU32));
    assert_lt(off, 16);
    char *refbuf = raw_refbuf.wbuf() + off, *refbuf2 = NULL;
    
    // discover a splice site, an insertion, or a deletion
    index_t maxscorei = (index_t)OFF_MASK;
    int64_t maxscore = MIN_I64;
    uint32_t maxspldir = EDIT_SPL_UNKNOWN;
    float maxsplscore = 0.0f;
    // allow an indel near a splice site
    index_t splice_gap_maxscorei = (index_t)OFF_MASK;
    int64_t donor_seq = 0, acceptor_seq = 0;
    int splice_gap_off = 0;
    if(spliced || ins || del) {
        int other_ref_ext = min<int>(read_gaps + (int)intronic_len, other_toff + other_len - len);
        SStringExpandable<char>& raw_refbuf2 = _sharedVars->raw_refbuf2;
        raw_refbuf2.resize(len + other_ref_ext + 16);
        int off2 = ref.getStretch(
                                  reinterpret_cast<uint32_t*>(raw_refbuf2.wbuf()),
                                  (size_t)otherHit._tidx,
                                  (size_t)(other_toff + other_len - len - other_ref_ext),
                                  len + other_ref_ext
                                  ASSERT_ONLY(, destU32));
        refbuf2 = raw_refbuf2.wbuf() + off2 + other_ref_ext;
        temp_scores.resize(len);
        temp_scores2.resize(len);
        if(spliced) {
            static const char GT   = 0x23, AG   = 0x02;
            static const char GTrc = 0x01, AGrc = 0x13;
            static const char GC   = 0x21, GCrc = 0x21;
            static const char AT   = 0x03, AC   = 0x01;
            static const char ATrc = 0x03, ACrc = 0x20;
            int i;
            for(i = 0; i < (int)len; i++) {
                int rdc = seq[this_rdoff + i], rfc = refbuf[i];
                if(i > 0) {
                    temp_scores[i] = temp_scores[i-1];
                } else {
                    temp_scores[i] = 0;
                }
                if(rdc != rfc) {
                    temp_scores[i] += sc.score(rdc, 1 << rfc, qual[this_rdoff + i] - 33);
                }
                if(temp_scores[i] < remainsc) {
                    break;
                }
            }
            int i_limit = min<int>(i, len);
            int i2;
            for(i2 = len - 1; i2 >= 0; i2--) {
                int rdc = seq[this_rdoff + i2], rfc = refbuf2[i2];
                if((index_t)(i2 + 1) < len) {
                    temp_scores2[i2] = temp_scores2[i2+1];
                } else {
                    temp_scores2[i2] = 0;
                }
                if(rdc != rfc) {
                    temp_scores2[i2] += sc.score(rdc, 1 << rfc, qual[this_rdoff + i2] - 33);
                }
                if(temp_scores2[i2] < remainsc) {
                    break;
                }
            }
            int i2_limit = max<int>(i2, 0);
            if(spliceSite != NULL){
                assert_leq(this_toff, (int)spliceSite->left());
                if(i2_limit <= (int)(spliceSite->left() - this_toff)) {
                    i2_limit = (int)(spliceSite->left() - this_toff);
                    i_limit = i2_limit + 1;
                } else {
                    i_limit = i2_limit;
                }
            }
            for(i = i2_limit, i2 = i2_limit + 1;
                i < i_limit && i2 < (int)len;
                i++, i2++) {
                int64_t tempscore = temp_scores[i] + temp_scores2[i2];
                char donor = 0xff, acceptor = 0xff;
                if((index_t)(i + 2) < len + this_ref_ext) {
                    donor = refbuf[i + 1];
                    donor = (donor << 4) | refbuf[i + 2];
                }
                if(i2 - 2 >= -other_ref_ext) {
                    acceptor = refbuf2[i2 - 2];
                    acceptor = (acceptor << 4) | refbuf2[i2 - 1];
                }
                uint32_t spldir = EDIT_SPL_UNKNOWN;
                if((donor == GT && acceptor == AG) /* || (donor == AT && acceptor == AC) */) {
                    spldir = EDIT_SPL_FW;
                } else if((donor == AGrc && acceptor == GTrc) /* || (donor == ACrc && acceptor == ATrc) */) {
                    spldir = EDIT_SPL_RC;
                }
                bool semi_canonical = (donor == GC && acceptor == AG) || (donor == AT && acceptor == AC) ||
                (donor == AGrc && acceptor == GCrc) || (donor == ACrc && acceptor == ATrc);
                tempscore -= (spldir == EDIT_SPL_UNKNOWN ? sc.noncanSpl() : sc.canSpl());
                int64_t temp_donor_seq = 0, temp_acceptor_seq = 0;
                float splscore = 0.0f;
                if(spldir != EDIT_SPL_UNKNOWN) {
                    // in case of canonical splice site, extract donor side sequence and acceptor side sequence
                    //    to calculate a score of the splicing event.
                    if(spldir == EDIT_SPL_FW) {
                        if(i + 1 >= (int)donor_exonic_len &&
                           (int)(len + this_ref_ext) > i + (int)donor_intronic_len &&
                           i2 + (int)other_ref_ext >= (int)acceptor_intronic_len &&
                           (int)len > i2 + (int)acceptor_exonic_len - 1) {
                            int from = i + 1 - (int)donor_exonic_len;
                            int to = i + (int)donor_intronic_len;
                            for(int j = from; j <= to; j++) {
                                assert_geq(j, 0);
                                assert_lt(j, (int)(len + this_ref_ext));
                                int base = refbuf[j];
                                if(base > 3) base = 0;
                                temp_donor_seq = temp_donor_seq << 2 | base;
                            }
                            from = i2 - acceptor_intronic_len;
                            to = i2 + acceptor_exonic_len - 1;
                            for(int j = from; j <= to; j++) {
                                assert_geq(j, -(int)other_ref_ext);
                                assert_lt(j, (int)len);
                                int base = refbuf2[j];
                                if(base > 3) base = 0;
                                temp_acceptor_seq = temp_acceptor_seq << 2 | base;
                            }
                        }
                    } else if(spldir == EDIT_SPL_RC) {
                        if(i + 1 >= (int)acceptor_exonic_len &&
                           (int)(len + this_ref_ext) > i + (int)acceptor_intronic_len &&
                           i2 + (int)other_ref_ext >= (int)donor_intronic_len &&
                           (int)len > i2 + (int)donor_exonic_len - 1) {
                            int from = i + 1 - (int)acceptor_exonic_len;
                            int to = i + (int)acceptor_intronic_len;
                            for(int j = to; j >= from; j--) {
                                assert_geq(j, 0);
                                assert_lt(j, (int)(len + this_ref_ext));
                                int base = refbuf[j];
                                if(base > 3) base = 0;
                                temp_acceptor_seq = temp_acceptor_seq << 2 | (base ^ 0x3);
                            }
                            from = i2 - donor_intronic_len;
                            to = i2 + donor_exonic_len - 1;
                            for(int j = to; j >= from; j--) {
                                assert_geq(j, -(int)other_ref_ext);
                                assert_lt(j, (int)len);
                                int base = refbuf2[j];
                                if(base > 3) base = 0;
                                temp_donor_seq = temp_donor_seq << 2 | (base ^ 0x3);
                            }
                        }
                    }
                    
                    splscore = SpliceSiteDB::probscore(temp_donor_seq, temp_acceptor_seq);
                }
                // daehwan - for debugging purposes
                // choose a splice site with the better score
                if((maxspldir == EDIT_SPL_UNKNOWN && spldir == EDIT_SPL_UNKNOWN && maxscore < tempscore) ||
                   (maxspldir == EDIT_SPL_UNKNOWN && spldir == EDIT_SPL_UNKNOWN && maxscore == tempscore && semi_canonical) ||
                   (maxspldir != EDIT_SPL_UNKNOWN && spldir != EDIT_SPL_UNKNOWN && (maxscore < tempscore || (maxscore == tempscore && maxsplscore < splscore))) ||
                   (maxspldir == EDIT_SPL_UNKNOWN && spldir != EDIT_SPL_UNKNOWN)) {
                    maxscore = tempscore;
                    maxscorei = i;
                    maxspldir = spldir;
                    maxsplscore = splscore;
                    if(maxspldir != EDIT_SPL_UNKNOWN) {
                        donor_seq = temp_donor_seq;
                        acceptor_seq = temp_acceptor_seq;
                    } else {
                        donor_seq = 0;
                        acceptor_seq = 0;
                    }
                }
            }
            // daehwan - do not allow indels near splice sites
#if 0
            if(maxscore < -sc.canSpl() - min<int64_t>(sc.readGapOpen(), sc.refGapOpen())) {
                int j1_start = 0, j2_start = len - 1;
                for(int i = 0; i < (int)len + this_ref_ext - 2; i++) {
                    assert_lt(i + 2, (int)len + this_ref_ext);
                    char donor = refbuf[i + 1] << 4 | refbuf[i + 2];
                    bool donor_canonical = (donor == GT || donor == AGrc);
                    if(!donor_canonical) continue;
                    int64_t remainsc2 = max<int64_t>(remainsc, maxscore);
                    int temp_read_gaps = sc.maxReadGaps(remainsc2 + sc.canSpl(), rdlen);
                    int temp_ref_gaps = sc.maxRefGaps(remainsc2 + sc.canSpl(), rdlen);
                    for(int gap_off = -temp_read_gaps; gap_off <= temp_ref_gaps; gap_off++) {
                        if(gap_off == 0) continue;
                        int i2 = i + 1 + gap_off;
                        if(i2 - 2 < -other_ref_ext || i2 >= (int)len) continue;
                        char acceptor = refbuf2[i2 - 2] << 4 | refbuf2[i2 - 1];
                        bool GTAG = (donor == GT && acceptor == AG) || (donor == AGrc && acceptor == GTrc);
                        bool canonical = GTAG;
                        if(!canonical) continue;
                        int rd_gap_off = -min<int>(gap_off, 0);
                        int ref_gap_off = max<int>(gap_off, 0);
                        int64_t temp_score = (canonical ? -sc.canSpl() : -sc.noncanSpl());
                        if(rd_gap_off > 0) {
                            temp_score = -(sc.readGapOpen() + sc.readGapExtend() * (rd_gap_off - 1));
                        } else if(ref_gap_off > 0) {
                            temp_score = -(sc.refGapOpen() + sc.refGapExtend() * (ref_gap_off - 1));
                        }
                        if(temp_score < remainsc2) continue;
                        int j;
                        for(j = j1_start; j < (int)len; j++) {
                            int rdc_off = this_rdoff + j;
                            assert_lt(rdc_off, (int)rdlen);
                            int rdc = seq[rdc_off];
                            int qv = qual[rdc_off];
                            int rfc = (j <= i ? refbuf[j] : refbuf2[j + ref_gap_off - rd_gap_off]);
                            if((int)j > 0) {
                                temp_scores[j] = temp_scores[j-1];
                            } else {
                                temp_scores[j] = 0;
                            }
                            if(rdc != rfc) {
                                temp_scores[j] += sc.score(rdc, 1 << rfc, qv - 33);
                            }
                            if(temp_scores[j] + temp_score < remainsc2) {
                                break;
                            }
                        }
                        j1_start = min<int>(i, j);
                        int j_limit = min<int>(j, len);
                        int j2;
                        for(j2 = max<int>(i2, j2_start); j2 >= 0; j2--) {
                            int rdc_off = this_rdoff + j2;
                            assert_lt(rdc_off, (int)rdlen);
                            int rdc = seq[rdc_off];
                            int qv = qual[rdc_off];
                            int rfc = (j2 >= i2 ? refbuf2[j2] : refbuf[j2 - ref_gap_off + rd_gap_off]);
                            if((index_t)(j2 + 1) < len) {
                                temp_scores2[j2] = temp_scores2[j2+1];
                            } else {
                                temp_scores2[j2] = 0;
                            }
                            if(rdc != rfc) {
                                temp_scores2[j2] += sc.score(rdc, 1 << rfc, qv - 33);
                            }
                            if(temp_scores2[j2] + temp_score < remainsc2) {
                                break;
                            }
                        }
                        j2_start = max<int>(i2, j2) - 1;
                        int j2_limit = (j2 < ref_gap_off ? 0 : j2 - ref_gap_off);
                        assert_geq(j2_limit, 0);
                        int64_t max_gap_score = MIN_I64;
                        index_t max_gap_j = (index_t)OFF_MASK;
                        for(j = j2_limit, j2 = j2_limit + 1 + ref_gap_off;
                            j < j_limit && j2 < (int)len;
                            j++, j2++) {
                            int64_t temp_gap_score = temp_scores[j] + temp_scores2[j2];
                            if(max_gap_score < temp_gap_score) {
                                max_gap_score = temp_gap_score;
                                max_gap_j = j;
                            }
                        }
                        if(max_gap_j == (index_t)OFF_MASK) continue;
                        temp_score += max_gap_score;
                        index_t temp_maxscorei = ((int)max_gap_j < i ? i + ref_gap_off - rd_gap_off : i);
                        if(temp_maxscorei <= 0 || temp_maxscorei >= len - 1) continue;
                        if(maxscore < temp_score) {
                            maxscore = temp_score;
                            maxscorei = temp_maxscorei;
                            splice_gap_maxscorei = max_gap_j;
                            splice_gap_off = gap_off;
                            maxspldir = (donor == GT ? EDIT_SPL_FW : EDIT_SPL_RC);
                        }
                    }
                }
            }
#endif
        } else {
            // discover an insertion or a deletion
            assert(ins || del);
            int inslen = (ins ? rddif - refdif : 0);
            int dellen = (del ? refdif - rddif : 0);
            int64_t gap_penalty;
            if(ins) {
                gap_penalty = -(sc.refGapOpen() + sc.refGapExtend() * (inslen - 1));
            } else {
                assert(del);
                gap_penalty = -(sc.readGapOpen() + sc.readGapExtend() * (dellen - 1));
            }
            if(gap_penalty < remainsc) return false;
            int i;
            for(i = 0; i < (int)len; i++) {
                int rdc = seq[this_rdoff + i], rfc = refbuf[i];
                if(i > 0) {
                    temp_scores[i] = temp_scores[i-1];
                } else {
                    temp_scores[i] = 0;
                }
                if(rdc != rfc) {
                    temp_scores[i] += sc.score(rdc, 1 << rfc, qual[this_rdoff + i] - 33);
                }
                if(temp_scores[i] + gap_penalty < remainsc) {
                    break;
                }
            }
            int i_limit = min<int>(i, len);
            int i2;
            for(i2 = len - 1; i2 >= 0; i2--) {
                int rdc = seq[this_rdoff + i2], rfc = refbuf2[i2];
                if((index_t)(i2 + 1) < len) {
                    temp_scores2[i2] = temp_scores2[i2+1];
                } else {
                    temp_scores2[i2] = 0;
                }
                if(rdc != rfc) {
                    temp_scores2[i2] += sc.score(rdc, 1 << rfc, qual[this_rdoff + i2] - 33);
                }
                if(temp_scores2[i2] + gap_penalty < remainsc) {
                    break;
                }
            }
            int i2_limit = (i2 < inslen ? 0 : i2 - inslen);
            for(i = i2_limit, i2 = i2_limit + 1 + inslen;
                i < i_limit && i2 < (int)len;
                i++, i2++) {
                int64_t tempscore = temp_scores[i] + temp_scores2[i2] + gap_penalty;
                if(maxscore < tempscore) {
                    maxscore = tempscore;
                    maxscorei = i;
                }
            }
        }
        if(maxscore == MIN_I64) return false;
        assert_lt(maxscorei, len);
        if(spliced && spliceSite == NULL) {
            uint32_t shorter_anchor_len = min<uint32_t>(maxscorei + 1, len - maxscorei - 1);
            assert_leq(this_toff, other_toff);
            if(maxspldir == EDIT_SPL_UNKNOWN) {
                if(shorter_anchor_len < noncan_mal) {
                    float intronLenProb = intronLen_prob_noncan(shorter_anchor_len, other_toff - this_toff, maxIntronLen);
                    if(intronLenProb > 0.01f)
                        return false;
                }
            } else {
                if(shorter_anchor_len < can_mal) {
                    float intronLenProb = intronLen_prob(shorter_anchor_len, other_toff - this_toff, maxIntronLen);
                    if(intronLenProb > 0.01f)
                        return false;
                }
            }
        }
        if(maxscore < remainsc)
            return false;
    }
    
    bool clear = true;
    for(int i = _edits->size() - 1; i >= 0; i--) {
        const Edit& edit = (*_edits)[i];
        if(edit.type == EDIT_TYPE_SPL ||
           edit.type == EDIT_TYPE_READ_GAP ||
           edit.type == EDIT_TYPE_REF_GAP) {
            _edits->resize(i+1);
            clear = false;
            break;
        }
    }
    if(clear) this->_edits->clear();
    // combine two alignments while updating edits
    if(spliced) {
        assert_geq(this_rdoff, this->_rdoff);
        index_t addoff = this_rdoff - this->_rdoff;
        int rd_gap_off = -min<int>(splice_gap_off, 0);
        int ref_gap_off = max<int>(splice_gap_off, 0);
        for(int i = 0; i < (int)len; i++) {
            assert_lt(this_rdoff + i, rdlen);
            int rdc = seq[this_rdoff + i];
            assert_range(0, 4, rdc);
            int rfc;
            if(splice_gap_maxscorei <= maxscorei) {
	      if(i <= (int)splice_gap_maxscorei) {
                    rfc = refbuf[i];
	      } else if(i <= (int)maxscorei) {
                    rfc = refbuf[i - ref_gap_off + rd_gap_off];
                } else {
                    rfc = refbuf2[i];
                }
            } else {
	      if(i <= (int)maxscorei) {
                    rfc = refbuf[i];
	      } else if(i <= (int)splice_gap_maxscorei) {
                    rfc = refbuf2[i + ref_gap_off - rd_gap_off];
                } else {
                    rfc = refbuf2[i];
                }
            }
            assert_range(0, 4, rfc);
            if(rdc != rfc) {
                Edit e((uint32_t)(i + addoff), rfc, rdc, EDIT_TYPE_MM, false);
                _edits->push_back(e);
            }
            if(i == (int)maxscorei) {
                index_t left = this_toff + i + 1;
                if(splice_gap_maxscorei <= maxscorei) {
                    left = left - ref_gap_off + rd_gap_off;
                }
                index_t right = other_toff + other_len - (len - i - 1);
                if(splice_gap_maxscorei > maxscorei) {
                    right = right + ref_gap_off - rd_gap_off;
                }
                index_t skipLen = 0;
                assert_lt(left, right);
                skipLen = right - left;
                Edit e((uint32_t)(i + 1 + addoff), 0, 0, EDIT_TYPE_SPL, skipLen, maxspldir, spliceSite != NULL, false);
                e.donor_seq = donor_seq;
                e.acceptor_seq = acceptor_seq;
                _edits->push_back(e);
            }
            if(i == (int)splice_gap_maxscorei && splice_gap_off != 0) {
                if(rd_gap_off > 0) {
                    assert_lt(left, right);
                    for(index_t j = 0; j < (index_t)rd_gap_off; j++) {
                        int temp_rfc_off = i + 1 + j;
                        int temp_rfc;
                        if(i < (int)maxscorei) {
                            temp_rfc = refbuf[temp_rfc_off];
                        } else {
                            temp_rfc = refbuf2[temp_rfc_off - rd_gap_off];
                        }
                        assert_range(0, 4, temp_rfc);
                        Edit e((uint32_t)(i + 1 + addoff), "ACGTN"[temp_rfc], '-', EDIT_TYPE_READ_GAP);
                        _edits->push_back(e);
                    }
                } else {
                    assert_gt(ref_gap_off, 0);
                    for(index_t j = 0; j < (index_t)ref_gap_off; j++) {
                        assert_lt(this_rdoff + i + 1 + j, rdlen);
                        int temp_rdc = seq[this_rdoff + i + 1 + j];
                        assert_range(0, 4, temp_rdc);
                        Edit e((uint32_t)(i + 1 + j + addoff), '-', "ACGTN"[temp_rdc], EDIT_TYPE_REF_GAP);
                        _edits->push_back(e);
                    }
                    i += ref_gap_off;
                }
            }
        }
    } else {
        for(index_t i = 0; i < len; i++) {
            char rdc = seq[this_rdoff + i];
            char rfc = (i <= maxscorei ? refbuf[i] : refbuf2[i]);
            assert_geq(this_rdoff, this->_rdoff);
            index_t addoff = this_rdoff - this->_rdoff;
            if(rdc != rfc) {
                Edit e((uint32_t)(i + addoff), rfc, rdc, EDIT_TYPE_MM, false);
                _edits->push_back(e);
            }
            if(i == maxscorei) {
                index_t left = this_toff + i + 1;
                index_t right = other_toff + other_len - (len - i - 1);
                index_t skipLen = 0;
                if(del) {
                    assert_lt(left, right);
                    skipLen = right - left;
                    assert_leq(skipLen, maxDelLen);
                    for(index_t j = 0; j < skipLen; j++) {
                        int temp_rfc;
                        if(i + 1 + j < len) temp_rfc = refbuf[i + 1 + j];
                        else                temp_rfc = ref.getBase(this->_tidx, this_toff + i + 1 + j);
                        assert_range(0, 4, temp_rfc);
                        Edit e((uint32_t)(i + 1 + addoff), "ACGTN"[temp_rfc], '-', EDIT_TYPE_READ_GAP);
                        _edits->push_back(e);
                    }
                } else {
                    assert(ins);
                    assert_lt(right, left);
                    skipLen = left - right;
                    assert_leq(skipLen, maxInsLen);
                    for(index_t j = 0; j < skipLen; j++) {
                        assert_lt(this_rdoff + i + 1 + j, seq.length());
                        int temp_rdc = seq[this_rdoff + i + 1 + j];
                        assert_range(0, 4, temp_rdc);
                        Edit e((uint32_t)(i + 1 + j + addoff), '-', "ACGTN"[temp_rdc], EDIT_TYPE_REF_GAP);
                        _edits->push_back(e);
                    }
                    i += skipLen;
                }
            }
        }
    }
    index_t fsi = otherHit._edits->size();
    for(index_t i = 0; i < otherHit._edits->size(); i++) {
        const Edit& edit = (*otherHit._edits)[i];
        if(edit.type == EDIT_TYPE_SPL ||
           edit.type == EDIT_TYPE_READ_GAP ||
           edit.type == EDIT_TYPE_REF_GAP) {
            fsi = i;
            break;
        }
    }
    assert_leq(this->_rdoff, otherHit._rdoff);
    index_t addoff = otherHit._rdoff - this->_rdoff;
    for(index_t i = fsi; i < otherHit._edits->size(); i++) {
        _edits->push_back((*otherHit._edits)[i]);
        _edits->back().pos += addoff;
    }
    // for alignment involving indel, left align so that
    // indels go to the left most of the combined alignment
    if(ins || del || (spliced && splice_gap_off != 0)) {
        leftAlign(rd);
    }
    
    // update alignment score, trims
    assert_leq(this->_rdoff + this->_len, otherHit._rdoff + otherHit._len);
    _len = otherHit._rdoff + otherHit._len - this->_rdoff;
    _score = calculateScore(
                            rd,
                            ssdb,
                            sc,
                            minK_local,
                            minIntronLen,
                            maxIntronLen,
                            ref);
    assert_eq(_trim3, 0);
    _trim3 += otherHit._trim3;
    assert(repOk(rd, ref));
    return true;
}

/**
 * Extend the partial alignment (GenomeHit) bidirectionally
 */
template <typename index_t>
bool GenomeHit<index_t>::extend(
                                const Read&             rd,
                                const BitPairReference& ref,
                                SpliceSiteDB&           ssdb,
                                SwAligner&              swa,
                                SwMetrics&              swm,
                                PerReadMetrics&         prm,
                                const Scoring&          sc,
                                TAlScore                minsc,
                                RandomSource&           rnd,           // pseudo-random source
                                index_t                 minK_local,
                                index_t                 minIntronLen,
                                index_t                 maxIntronLen,
                                index_t&                leftext,
                                index_t&                rightext,
                                index_t                 mm)
{
    assert_lt(this->_tidx, ref.numRefs());
    index_t max_leftext = leftext, max_rightext = rightext;
    assert(max_leftext > 0 || max_rightext > 0);
    leftext = 0, rightext = 0;
    index_t rdlen = (index_t)rd.length();
    bool doLeftAlign = false;
    
    assert(_sharedVars != NULL);
    SStringExpandable<char>& raw_refbuf = _sharedVars->raw_refbuf;
    ASSERT_ONLY(SStringExpandable<uint32_t>& destU32 = _sharedVars->destU32);
    
    // extend the alignment further in the left direction
    // with 'mm' mismatches allowed
    const BTDnaString& seq = _fw ? rd.patFw : rd.patRc;
    const BTString& qual = _fw ? rd.qual  : rd.qualRev;
    if(max_leftext > 0 && _rdoff > 0) {
        assert_gt(_rdoff, 0);
        index_t left_rdoff, left_len, left_toff;
        this->getLeft(left_rdoff, left_len, left_toff);
        assert_gt(left_len, 0);
        assert_eq(left_rdoff, _rdoff);
        assert_eq(left_toff, _toff);
        if(_rdoff > _toff) return false;
        assert_leq(_rdoff, _toff);
        index_t rl = _toff - _rdoff;
        index_t reflen = ref.approxLen(_tidx);
        assert_geq(_score, minsc);
        int read_gaps = sc.maxReadGaps(minsc - _score, rdlen);
        int ref_gaps = sc.maxRefGaps(minsc - _score, rdlen);
        
        // daehwan - for debugging purposes
        if(mm <= 0 || true) {
            read_gaps = ref_gaps = 0;
        }
        if(read_gaps > (int)rl) {
            read_gaps = rl;
            rl = 0;
        } else {
            rl -= read_gaps;
        }
        ref_gaps = min<int>(_rdoff - 1, ref_gaps);
        if(rl + read_gaps + _rdoff <= reflen) {
            raw_refbuf.resize(rdlen + 16);
            int off = ref.getStretch(
                                     reinterpret_cast<uint32_t*>(raw_refbuf.wbuf()),
                                     (size_t)_tidx,
                                     (size_t)rl,
                                     _rdoff + read_gaps
                                     ASSERT_ONLY(, destU32));
            assert_lt(off, 16);
            char *refbuf = raw_refbuf.wbuf() + off;
            int best_gap_off = 0, best_ext = 0, best_score = MIN_I;
            for(int gap_off = -read_gaps; gap_off <= ref_gaps; gap_off++) {
                int rd_gap_off = min<int>(gap_off, 0);
                int ref_gap_off = -max<int>(gap_off, 0);
                int temp_ext = 0, temp_mm = 0, temp_score = 0, temp_mm_ext = 0;
                if(rd_gap_off < 0) {
                    temp_score -= (sc.readGapOpen() + sc.readGapExtend() * (-rd_gap_off - 1));
                } else if(ref_gap_off < 0) {
                    temp_score -= (sc.refGapOpen() + sc.refGapExtend() * (-ref_gap_off - 1));
                }
                while(temp_ext - ref_gap_off < (int)_rdoff && temp_ext - ref_gap_off < (int)minK_local) {
                    int rdc_off = _rdoff - temp_ext - 1 + ref_gap_off;
                    if(rdc_off < 0 || rdc_off >= (int)rdlen) break;
                    int rdc = seq[rdc_off];
                    int rfc_off = _rdoff - temp_ext - 1 + read_gaps + rd_gap_off;
                    assert_geq(rfc_off, 0);
                    assert_lt(rfc_off, (int)_rdoff + read_gaps);
                    int rfc = refbuf[rfc_off];
                    if(rdc != rfc) {
                        temp_mm++;
                        temp_score += sc.score(rdc, 1 << rfc, qual[rdc_off] - 33);
                    }
                    if(temp_mm <= (int)mm) temp_mm_ext++;
                    temp_ext++;
                }
                if(best_score < temp_score) {
                    best_gap_off = gap_off;
                    best_ext = temp_mm_ext;
                    best_score = temp_score;
                }
            }
            if(best_ext > 0) {
                index_t added_edit = 0;
                int rd_gap_off = min<int>(best_gap_off, 0);
                int ref_gap_off = -max<int>(best_gap_off, 0);
                assert(rd_gap_off == 0 || ref_gap_off == 0);
                if(rd_gap_off < 0) {
                    for(int i = -1; i >= rd_gap_off; i--) {
                        int rfc_off = _rdoff + read_gaps + i;
                        assert_geq(rfc_off, 0);
                        assert_lt(rfc_off, (int)_rdoff + read_gaps);
                        int rfc = refbuf[rfc_off];
                        Edit e(0, "ACGTN"[rfc], '-', EDIT_TYPE_READ_GAP);
                        _edits->insert(e, 0);
                        added_edit++;
                    }
                    doLeftAlign = true;
                } else if(ref_gap_off < 0) {
                    for(int i = -1; i >= ref_gap_off; i--) {
                        int rdc_off = _rdoff + i;
                        assert_geq(rdc_off, 0);
                        assert_lt(rdc_off, (int)rdlen);
                        int rdc = seq[rdc_off];
                        Edit e(-i, '-', "ACGTN"[rdc], EDIT_TYPE_REF_GAP);
                        _edits->insert(e, 0);
                        added_edit++;
                    }
                    doLeftAlign = true;
                }
                index_t left_mm = 0;
                while(leftext - ref_gap_off < _rdoff && leftext - ref_gap_off < max_leftext) {
                    int rdc_off = _rdoff - leftext - 1 + ref_gap_off;
                    assert_geq(rdc_off, 0);
                    assert_lt(rdc_off, (int)rdlen);
                    int rdc = seq[rdc_off];
                    int rfc_off = _rdoff - leftext - 1 + read_gaps + rd_gap_off;
                    assert_geq(rfc_off, 0);
                    assert_lt(rfc_off, (int)_rdoff + read_gaps);
                    int rfc = refbuf[rfc_off];
                    if(rdc != rfc) {
                        left_mm++;
                        if(left_mm > mm) break;
                        Edit e(leftext + 1 - ref_gap_off, rfc, rdc, EDIT_TYPE_MM, false);
                        _edits->insert(e, 0);
                        added_edit++;
                    }
                    leftext++;
                }
                leftext = leftext - ref_gap_off;
                if(leftext > 0) {
                    assert_leq(leftext, _rdoff);
                    assert_leq(leftext, _toff);
                    _toff -= (leftext + ref_gap_off - rd_gap_off);
                    _rdoff -= leftext;
                    _len += leftext;
                    for(index_t i = 0; i < _edits->size(); i++) {
                        if(i < added_edit) {
                            (*_edits)[i].pos = leftext - (*_edits)[i].pos;
                        } else {
                            (*_edits)[i].pos += leftext;
                        }
                    }
                }
            }
        }
    }
    
    // extend the alignment further in the right direction
    // with 'mm' mismatches allowed
    if(max_rightext > 0 && _rdoff + _len < rdlen) {
        index_t right_rdoff, right_len, right_toff;
        this->getRight(right_rdoff, right_len, right_toff);
        assert_gt(right_len, 0);
        index_t rl = right_toff + right_len;
        assert_eq(_rdoff + _len, right_rdoff + right_len);
        index_t rr = rdlen - (right_rdoff + right_len);
        index_t reflen = ref.approxLen(_tidx);
        int read_gaps = sc.maxReadGaps(minsc - _score, rdlen);
        int ref_gaps = sc.maxRefGaps(minsc - _score, rdlen);
        
        // daehwan - for debugging purposes
        if(mm <= 0 || true) {
            read_gaps = ref_gaps = 0;
        }
        if(rl + rr + read_gaps > reflen) {
            if(rl + rr >= reflen)   read_gaps = 0;
            else                    read_gaps = reflen - (rl + rr);
        }
        rr += read_gaps;
        ref_gaps = min<int>(rdlen - (_rdoff + _len) - 1, ref_gaps);
        if(rl + rr <= reflen) {
            raw_refbuf.resize(rdlen + 16);
            int off = ref.getStretch(
                                     reinterpret_cast<uint32_t*>(raw_refbuf.wbuf()),
                                     (size_t)_tidx,
                                     (size_t)rl,
                                     rr
                                     ASSERT_ONLY(, destU32));
            assert_lt(off, 16);
            char *refbuf = raw_refbuf.wbuf() + off;
            int best_gap_off = 0, best_ext = 0, best_score = MIN_I;
            for(int gap_off = -read_gaps; gap_off <= ref_gaps; gap_off++) {
                int rd_gap_off = -min<int>(gap_off, 0);
                int ref_gap_off = max<int>(gap_off, 0);
                int temp_ext = 0, temp_mm = 0, temp_score = 0, temp_mm_ext = 0;
                if(rd_gap_off > 0) {
                    temp_score -= (sc.readGapOpen() + sc.readGapExtend() * (rd_gap_off - 1));
                } else if(ref_gap_off > 0) {
                    temp_score -= (sc.refGapOpen() + sc.refGapExtend() * (ref_gap_off - 1));
                }
                while((int)_rdoff + (int)_len + temp_ext + ref_gap_off < (int)rdlen && temp_ext + ref_gap_off < (int)minK_local) {
                    int rdc_off = _rdoff + _len + temp_ext + ref_gap_off;
                    if(rdc_off < 0 || rdc_off >= (int)rdlen) break;
                    int rdc = seq[rdc_off];
                    int rfc_off = temp_ext + rd_gap_off;
                    assert_geq(rfc_off, 0);
                    assert_lt(rfc_off, (int)rr);
                    int rfc = refbuf[rfc_off];
                    if(rdc != rfc) {
                        temp_mm++;
                        temp_score += sc.score(rdc, 1 << rfc, qual[rdc_off] - 33);
                    }
                    if(temp_mm <= (int)mm) temp_mm_ext++;
                    temp_ext++;
                }

                if(best_score < temp_score) {
                    best_gap_off = gap_off;
                    best_ext = temp_mm_ext;
                    best_score = temp_score;
                }
            }
            
            if(best_ext > 0) {
                int rd_gap_off = -min<int>(best_gap_off, 0);
                int ref_gap_off = max<int>(best_gap_off, 0);
                assert(rd_gap_off == 0 || ref_gap_off == 0);
                if(rd_gap_off > 0) {
                    for(int i = 0; i < rd_gap_off; i++) {
                        int rfc_off = i;
                        assert_geq(rfc_off, 0);
                        assert_lt(rfc_off, (int)rr);
                        int rfc = refbuf[rfc_off];
                        Edit e(_len, "ACGTN"[rfc], '-', EDIT_TYPE_READ_GAP);
                        _edits->push_back(e);
                    }
                    doLeftAlign = true;
                } else if(ref_gap_off > 0) {
                    for(int i = 0; i < ref_gap_off; i++) {
                        int rdc_off = _rdoff + _len + i;
                        assert_geq(rdc_off, 0);
                        assert_lt(rdc_off, (int)rdlen);
                        int rdc = seq[rdc_off];
                        Edit e(_len + i, '-', "ACGTN"[rdc], EDIT_TYPE_REF_GAP);
                        _edits->push_back(e);
                    }
                    doLeftAlign = true;
                }
                index_t right_mm = 0;
                while(_rdoff + _len + rightext + ref_gap_off < rdlen && rightext + ref_gap_off < max_rightext) {
                    int rdc_off = _rdoff + _len + rightext + ref_gap_off;
                    assert_geq(rdc_off, 0);
                    assert_lt(rdc_off, (int)rdlen);
                    int rdc = seq[rdc_off];
                    int rfc_off = rightext + rd_gap_off;
                    assert_geq(rfc_off, 0);
                    assert_lt(rfc_off, (int)rr);
                    int rfc = refbuf[rfc_off];
                    if(rdc != rfc) {
                        right_mm++;
                        if(right_mm > mm) break;
                        Edit e(_len + rightext + ref_gap_off, rfc, rdc, EDIT_TYPE_MM, false);
                        _edits->push_back(e);
                    }
                    rightext++;
                }
                rightext += ref_gap_off;
                _len += rightext;
            }
        }
    }
    
    if(doLeftAlign) leftAlign(rd);
    assert_leq(_rdoff + _len, rdlen);
    _score = calculateScore(
                            rd,
                            ssdb,
                            sc,
                            minK_local,
                            minIntronLen,
                            maxIntronLen,
                            ref);
    assert(repOk(rd, ref));
    return leftext > 0 || rightext > 0;
}

/**
 * For alignment involving indel, move the indels
 * to the left most possible position
 */
template <typename index_t>
void GenomeHit<index_t>::leftAlign(const Read& rd)
{
    ASSERT_ONLY(const index_t rdlen = rd.length());
    const BTDnaString& seq = _fw ? rd.patFw : rd.patRc;
    for(index_t ei = 0; ei < _edits->size(); ei++) {
        Edit& edit = (*_edits)[ei];
        if(edit.type != EDIT_TYPE_READ_GAP && edit.type != EDIT_TYPE_REF_GAP)
            continue;
        
        index_t ei2 = ei + 1;
        for(; ei2 < _edits->size(); ei2++) {
            const Edit& edit2 = (*_edits)[ei2];
            if(edit2.type != edit.type) break;
            if(edit.type == EDIT_TYPE_READ_GAP) {
                if(edit.pos != edit2.pos) break;
            } else {
                assert_eq(edit.type, EDIT_TYPE_REF_GAP);
                if(edit.pos + ei2 - ei != edit2.pos) break;
            }
        }
        assert_gt(ei2, 0);
        ei2 -= 1;
        Edit& edit2 = (*_edits)[ei2];
        int b = 0;
        if(ei > 0) {
            const Edit& prev_edit = (*_edits)[ei - 1];
            b = prev_edit.pos;
        }
        int l = edit.pos - 1;
        while(l > b) {
            assert_lt(l, (int)rdlen);
            int rdc = seq[_rdoff + l];
            assert_range(0, 4, rdc);
            char rfc = (edit.type == EDIT_TYPE_READ_GAP ? edit2.chr : edit2.qchr);
            if(rfc != "ACGTN"[rdc]) break;
            for(int ei3 = ei2; ei3 > (int)ei; ei3--) {
                if(edit.type == EDIT_TYPE_READ_GAP) {
                    (*_edits)[ei3].chr = (*_edits)[ei3 - 1].chr;
                } else {
                    (*_edits)[ei3].qchr = (*_edits)[ei3 - 1].qchr;
                }
                (*_edits)[ei3].pos -= 1;
            }
            rdc = seq[_rdoff + l];
            assert_range(0, 4, rdc);
            if(edit.type == EDIT_TYPE_READ_GAP) {
                edit.chr = "ACGTN"[rdc];
            } else {
                edit.qchr = "ACGTN"[rdc];
            }
            edit.pos -= 1;
            l--;
        }
        ei = ei2;
    }
}

#ifndef NDEBUG
/**
 * Check that hit is sane w/r/t read.
 */
template <typename index_t>
bool GenomeHit<index_t>::repOk(const Read& rd, const BitPairReference& ref)
{
    assert(_sharedVars != NULL);
    SStringExpandable<char>& raw_refbuf = _sharedVars->raw_refbuf;
    SStringExpandable<uint32_t>& destU32 = _sharedVars->destU32;
    
    BTDnaString& editstr = _sharedVars->editstr;
    BTDnaString& partialseq = _sharedVars->partialseq;
    BTDnaString& refstr = _sharedVars->refstr;
    EList<index_t>& reflens = _sharedVars->reflens;
    EList<index_t>& refoffs = _sharedVars->refoffs;
    
    editstr.clear(); partialseq.clear(); refstr.clear();
    reflens.clear(); refoffs.clear();
    
    const BTDnaString& seq = _fw ? rd.patFw : rd.patRc;
    partialseq.install(seq.buf() + this->_rdoff, (size_t)this->_len);
    Edit::toRef(partialseq, *_edits, editstr);
    
    index_t refallen = 0;
    int64_t reflen = 0;
    int64_t refoff = this->_toff;
    refoffs.push_back(refoff);
    size_t eidx = 0;
    for(size_t i = 0; i < _len; i++, reflen++, refoff++) {
        while(eidx < _edits->size() && (*_edits)[eidx].pos == i) {
            const Edit& edit = (*_edits)[eidx];
            if(edit.isReadGap()) {
                reflen++;
                refoff++;
            } else if(edit.isRefGap()) {
                reflen--;
                refoff--;
            }
            if(edit.isSpliced()) {
                assert_gt(reflen, 0);
                refallen += reflen;
                reflens.push_back((index_t)reflen);
                reflen = 0;
                refoff += edit.splLen;
                assert_gt(refoff, 0);
                refoffs.push_back((index_t)refoff);
            }
            eidx++;
        }
    }
    assert_gt(reflen, 0);
    refallen += (index_t)reflen;
    reflens.push_back(reflen);
    assert_gt(reflens.size(), 0);
    assert_gt(refoffs.size(), 0);
    assert_eq(reflens.size(), refoffs.size());
    refstr.clear();
    for(index_t i = 0; i < reflens.size(); i++) {
        assert_gt(reflens[i], 0);
        if(i > 0) {
            assert_gt(refoffs[i], refoffs[i-1]);
        }
        raw_refbuf.resize(reflens[i] + 16);
        raw_refbuf.clear();
        int off = ref.getStretch(
                                 reinterpret_cast<uint32_t*>(raw_refbuf.wbuf()),
                                 (size_t)this->_tidx,
                                 (size_t)max<TRefOff>(refoffs[i], 0),
                                 reflens[i],
                                 destU32);
        assert_leq(off, 16);
        for(index_t j = 0; j < reflens[i]; j++) {
            char rfc = *(raw_refbuf.buf()+off+j);
            refstr.append(rfc);
        }
    }
    if(refstr != editstr) {
        cerr << "Decoded nucleotides and edits don't match reference:" << endl;
        //cerr << "           score: " << score.score()
        //<< " (" << gaps << " gaps)" << endl;
        cerr << "           edits: ";
        Edit::print(cerr, *_edits);
        cerr << endl;
        cerr << "    decoded nucs: " << partialseq << endl;
        cerr << "     edited nucs: " << editstr << endl;
        cerr << "  reference nucs: " << refstr << endl;
        assert(0);
    }

    return true;
}
#endif

/**
 * Calculate alignment score
 */
template <typename index_t>
int64_t GenomeHit<index_t>::calculateScore(
                                           const Read&             rd,
                                           SpliceSiteDB&           ssdb,
                                           const Scoring&          sc,
                                           index_t                 minK_local,
                                           index_t                 minIntronLen,
                                           index_t                 maxIntronLen,
                                           const BitPairReference& ref)
{
    int64_t score = 0;
    double splicescore = 0;
    index_t numsplices = 0;
    index_t mm = 0;
    const BTDnaString& seq = _fw ? rd.patFw : rd.patRc;
    const BTString& qual = _fw ? rd.qual : rd.qualRev;
    index_t rdlen = seq.length();
    index_t toff_base = _toff;
    bool conflict_splicesites = false;
    uint8_t whichsense = EDIT_SPL_UNKNOWN;
    for(index_t i = 0; i < _edits->size(); i++) {
        const Edit& edit = (*_edits)[i];
        assert_lt(edit.pos, _len);
        if(edit.type == EDIT_TYPE_MM) {
            int pen = sc.score(
                               dna2col[edit.qchr] - '0',
                               asc2dnamask[edit.chr],
                               qual[this->_rdoff + edit.pos] - 33);
            score += pen;
            mm++;
        } else if(edit.type == EDIT_TYPE_SPL) {
            // int left = toff_base + edit.pos - 1;
            // assert_geq(left, 0);
            // int right = left + edit.splLen + 1;
            // assert_geq(right, 0);
            if(!edit.knownSpl) {
                int left_anchor_len = _rdoff + edit.pos;
                assert_gt(left_anchor_len, 0);
                assert_lt(left_anchor_len, (int)rdlen);
                int right_anchor_len = rdlen - left_anchor_len;
                index_t mm2 = 0;
                for(index_t j = i + 1; j < _edits->size(); j++) {
                    const Edit& edit2 = (*_edits)[j];
                    if(edit2.type == EDIT_TYPE_MM ||
                       edit2.type == EDIT_TYPE_READ_GAP ||
                       edit2.type == EDIT_TYPE_REF_GAP) mm2++;
                }
                left_anchor_len -= (mm * 2);
                right_anchor_len -= (mm2 * 2);
                int shorter_anchor_len = min<int>(left_anchor_len, right_anchor_len);
                if(shorter_anchor_len <= 0) shorter_anchor_len = 1;
                assert_gt(shorter_anchor_len, 0);
                uint32_t intronLen_thresh = (edit.splDir != EDIT_SPL_UNKNOWN ? MaxIntronLen(shorter_anchor_len) : MaxIntronLen_noncan(shorter_anchor_len));
                if(intronLen_thresh < maxIntronLen) {
                    if(edit.splLen > intronLen_thresh) {
                        return -1000.0;
                    }
                    
                    if(edit.splDir != EDIT_SPL_UNKNOWN) {
                        float probscore = ssdb.probscore(edit.donor_seq, edit.acceptor_seq);
                        
                        float probscore_thresh = 0.8f;
                        if(edit.splLen >> 16) probscore_thresh = 0.99f;
                        else if(edit.splLen >> 15) probscore_thresh = 0.97f;
                        else if(edit.splLen >> 14) probscore_thresh = 0.94f;
                        else if(edit.splLen >> 13) probscore_thresh = 0.91f;
                        else if(edit.splLen >> 12) probscore_thresh = 0.88f;
                        if(probscore < probscore_thresh) return -1000.0;
                    }
                    if(shorter_anchor_len == left_anchor_len) {
                        if(_trim5 > 0) return -1000.0;
                        for(int j = (int)i - 1; j >= 0; j--) {
                            if((*_edits)[j].type == EDIT_TYPE_MM ||
                               (*_edits)[j].type == EDIT_TYPE_READ_GAP ||
                               (*_edits)[j].type == EDIT_TYPE_REF_GAP)
                                return -1000.0;
                        }
                    } else {
                        if(_trim3 > 0) return -1000.0;
                        for(index_t j = i + 1; j < _edits->size(); j++) {
                            if((*_edits)[j].type == EDIT_TYPE_MM ||
                               (*_edits)[j].type == EDIT_TYPE_READ_GAP ||
                               (*_edits)[j].type == EDIT_TYPE_REF_GAP)
                                return -1000.0;
                        }
                    }
                }

                if(edit.splDir != EDIT_SPL_UNKNOWN) {
                    score -= sc.canSpl((int)edit.splLen);
                } else {
                    score -= sc.noncanSpl((int)edit.splLen);
                }
                
                // daehwan - for debugging purposes
                if(shorter_anchor_len <= 15) {
                    numsplices += 1;
                    splicescore += (double)edit.splLen;
                }
            }
            
            if(!conflict_splicesites) {
                if(whichsense == EDIT_SPL_UNKNOWN) {
                    whichsense = edit.splDir;
                } else if(edit.splDir != EDIT_SPL_UNKNOWN) {
                    assert_neq(whichsense, EDIT_SPL_UNKNOWN);
                    if(whichsense != edit.splDir) {
                        conflict_splicesites = true;
                    }
                }
            }
            
            toff_base += edit.splLen;
        } else if(edit.type == EDIT_TYPE_READ_GAP) {
            bool open = true;
            if(i > 0 &&
               (*_edits)[i-1].type == EDIT_TYPE_READ_GAP &&
               (*_edits)[i-1].pos == edit.pos) {
                open = false;
            }
            if(open)    score -= sc.readGapOpen();
            else        score -= sc.readGapExtend();
            toff_base++;
        } else if(edit.type == EDIT_TYPE_REF_GAP) {
            bool open = true;
            if(i > 0 &&
               (*_edits)[i-1].type == EDIT_TYPE_REF_GAP &&
               (*_edits)[i-1].pos + 1 == edit.pos) {
                open = false;
            }
            if(open)    score -= sc.refGapOpen();
            else        score -= sc.refGapExtend();
            assert_gt(toff_base, 0);
            toff_base--;
        }
#ifndef NDEBUG
        else {
            assert(false);
        }
#endif
    }
    
    if(conflict_splicesites) {
        score -= sc.conflictSpl();
    }
    
    if (numsplices > 1) splicescore /= (double)numsplices;
    score += (_len - mm) * sc.match();
    _score = score;
    _splicescore = splicescore;
    
    return score;
}

/**
 * Encapsulates counters that measure how much work has been done by
 * hierarchical indexing
 */
struct HIMetrics {
    
	HIMetrics() : mutex_m() {
	    reset();
	}
    
	void reset() {
		anchoratts = 0;
        localatts = 0;
        localindexatts = 0;
        localextatts = 0;
        localsearchrecur = 0;
        globalgenomecoords = 0;
        localgenomecoords = 0;
	}
	
	void init(
              uint64_t localatts_,
              uint64_t anchoratts_,
              uint64_t localindexatts_,
              uint64_t localextatts_,
              uint64_t localsearchrecur_,
              uint64_t globalgenomecoords_,
              uint64_t localgenomecoords_)
	{
        localatts = localatts_;
        anchoratts = anchoratts_;
        localindexatts = localindexatts_;
        localextatts = localextatts_;
        localsearchrecur = localsearchrecur_;
        globalgenomecoords = globalgenomecoords_;
        localgenomecoords = localgenomecoords_;
    }
	
	/**
	 * Merge (add) the counters in the given HIMetrics object into this
	 * object.  This is the only safe way to update a HIMetrics shared
	 * by multiple threads.
	 */
	void merge(const HIMetrics& r, bool getLock = false) {
        ThreadSafe ts(&mutex_m, getLock);
        localatts += r.localatts;
        anchoratts += r.anchoratts;
        localindexatts += r.localindexatts;
        localextatts += r.localextatts;
        localsearchrecur += r.localsearchrecur;
        globalgenomecoords += r.globalgenomecoords;
        localgenomecoords += r.localgenomecoords;
    }
	   
    uint64_t localatts;      // # attempts of local search
    uint64_t anchoratts;     // # attempts of anchor search
    uint64_t localindexatts; // # attempts of local index search
    uint64_t localextatts;   // # attempts of extension search
    uint64_t localsearchrecur;
    uint64_t globalgenomecoords;
    uint64_t localgenomecoords;
	
	MUTEX_T mutex_m;
};

/**
 * With a hierarchical indexing, SplicedAligner provides several alignment strategies
 * , which enable effective alignment of RNA-seq reads
 */
template <typename index_t, typename local_index_t>
class HI_Aligner {

public:
	
	/**
	 * Initialize with index.
	 */
	HI_Aligner(
               const Ebwt<index_t>& ebwt,
               size_t minIntronLen = 20,
               size_t maxIntronLen = 500000,
               bool secondary = false,
               bool local = false,
               uint64_t threads_rids_mindist = 0,
               bool no_spliced_alignment = false) :
    _minIntronLen(minIntronLen),
    _maxIntronLen(maxIntronLen),
    _secondary(secondary),
    _local(local),
    _gwstate(GW_CAT),
    _gwstate_local(GW_CAT),
    _thread_rids_mindist(threads_rids_mindist),
    _no_spliced_alignment(no_spliced_alignment)
    {
        index_t genomeLen = ebwt.eh().len();
        _minK = 0;
        while(genomeLen > 0) {
            genomeLen >>= 2;
            _minK++;
        }
        _minK_local = 8;
    }
    
    HI_Aligner() {
    }
    
    /**
     */
    void initRead(Read *rd, bool nofw, bool norc, TAlScore minsc, TAlScore maxpen, bool rightendonly = false) {
        assert(rd != NULL);
        _rds[0] = rd;
        _rds[1] = NULL;
		_paired = false;
        _rightendonly = rightendonly;
        _nofw[0] = nofw;
        _nofw[1] = true;
        _norc[0] = norc;
        _norc[1] = true;
        _minsc[0] = minsc;
        _minsc[1] = OFF_MASK;
        _maxpen[0] = maxpen;
        _maxpen[1] = OFF_MASK;
        for(size_t fwi = 0; fwi < 2; fwi++) {
            bool fw = (fwi == 0);
            _hits[0][fwi].init(fw, _rds[0]->length());
        }
        _genomeHits.clear();
        _concordantPairs.clear();
        _hits_searched[0].clear();
        assert(!_paired);
    }
    
    /**
     */
    void initReads(Read *rds[2], bool nofw[2], bool norc[2], TAlScore minsc[2], TAlScore maxpen[2]) {
        assert(rds[0] != NULL && rds[1] != NULL);
		_paired = true;
        _rightendonly = false;
        for(size_t rdi = 0; rdi < 2; rdi++) {
            _rds[rdi] = rds[rdi];
            _nofw[rdi] = nofw[rdi];
            _norc[rdi] = norc[rdi];
            _minsc[rdi] = minsc[rdi];
            _maxpen[rdi] = maxpen[rdi];
            for(size_t fwi = 0; fwi < 2; fwi++) {
                bool fw = (fwi == 0);
		        _hits[rdi][fwi].init(fw, _rds[rdi]->length());
            }
            _hits_searched[rdi].clear();
        }
        _genomeHits.clear();
        _concordantPairs.clear();
        assert(_paired);
        assert(!_rightendonly);
    }
    
    /**
     * Aligns a read or a pair
     * This funcion is called per read or pair
     */
    virtual
    int go(
           const Scoring&           sc,
           const Ebwt<index_t>&     ebwtFw,
           const Ebwt<index_t>&     ebwtBw,
           const BitPairReference&  ref,
           SwAligner&               swa,
           SpliceSiteDB&            ssdb,
           WalkMetrics&             wlm,
           PerReadMetrics&          prm,
           SwMetrics&               swm,
           HIMetrics&               him,
           RandomSource&            rnd,
           AlnSinkWrap<index_t>&    sink)
    {
        index_t rdi;
        bool fw;
        bool found[2] = {true, this->_paired};
        // given read and its reverse complement
        //  (and mate and the reverse complement of mate in case of pair alignment),
        // pick up one with best partial alignment
        while(nextBWT(sc, ebwtFw, ebwtBw, ref, rdi, fw, wlm, prm, him, rnd, sink)) {
            // given the partial alignment, try to extend it to full alignments
        	found[rdi] = align(sc, ebwtFw, ebwtBw, ref, swa, ssdb, rdi, fw, wlm, prm, swm, him, rnd, sink);
            if(!found[0] && !found[1]) {
                break;
            }
            
            // try to combine this alignment with some of mate alignments
            // to produce pair alignment
            if(this->_paired) {
                pairReads(sc, ebwtFw, ebwtBw, ref, wlm, prm, him, rnd, sink);
                // if(sink.bestPair() >= _minsc[0] + _minsc[1]) break;
            }
        }
        
        // if no concordant pair is found, try to use alignment of one-end
        // as an anchor to align the other-end
        if(this->_paired) {
            if(_concordantPairs.size() == 0 &&
               (sink.bestUnp1() >= _minsc[0] || sink.bestUnp2() >= _minsc[1])) {
                bool mate_found = false;
                const EList<AlnRes> *rs[2] = {NULL, NULL};
                sink.getUnp1(rs[0]); assert(rs[0] != NULL);
                sink.getUnp2(rs[1]); assert(rs[1] != NULL);
                index_t rs_size[2] = {(index_t) rs[0]->size(), (index_t) rs[1]->size()};
                for(index_t i = 0; i < 2; i++) {
                    for(index_t j = 0; j < rs_size[i]; j++) {
                        const AlnRes& res = (*rs[i])[j];
                        bool fw = (res.orient() == 1);
                        mate_found |= alignMate(
                                                sc,
                                                ebwtFw,
                                                ebwtBw,
                                                ref,
                                                swa,
                                                ssdb,
                                                i,
                                                fw,
                                                wlm,
                                                prm,
                                                swm,
                                                him,
                                                rnd,
                                                sink,
                                                res.refid(),
                                                res.refoff());
                    }
                }
                
                if(mate_found) {
                    pairReads(sc, ebwtFw, ebwtBw, ref, wlm, prm, him, rnd, sink);
                }
            }
        }
        
        return EXTEND_POLICY_FULFILLED;
    }
    
    /**
     * Given a read or its reverse complement (or mate),
     * align the unmapped portion using the global FM index
     */
    virtual
    bool nextBWT(
                 const Scoring&          sc,
                 const Ebwt<index_t>&    ebwtFw,
                 const Ebwt<index_t>&    ebwtBw,
                 const BitPairReference& ref,
                 index_t&                rdi,
                 bool&                   fw,
                 WalkMetrics&            wlm,
                 PerReadMetrics&         prm,
                 HIMetrics&              him,
                 RandomSource&           rnd,
                 AlnSinkWrap<index_t>&   sink)
    {
        // pick up a candidate from a read or its reverse complement
        // (for pair, also consider mate and its reverse complement)
        while(pickNextReadToSearch(rdi, fw)) {
            size_t mineFw = 0, mineRc = 0;
            index_t fwi = (fw ? 0 : 1);
            ReadBWTHit<index_t>& hit = _hits[rdi][fwi];
            assert(!hit.done());
            bool pseudogeneStop = true, anchorStop = true;
            if(!_secondary) {
                index_t numSearched = hit.numActualPartialSearch();
                int64_t bestScore = 0;
                if(rdi == 0) {
                    bestScore = sink.bestUnp1();
                    if(bestScore >= _minsc[rdi]) {
                        // do not further align this candidate
                        // unless it may be at least as good as the alignment of its reverse complement
                        index_t maxmm = (-bestScore + sc.mmpMax - 1) / sc.mmpMax;
                        if(numSearched > maxmm + sink.bestSplicedUnp1() + 1) {
                            hit.done(true);
                            if(_paired) {
                                if(sink.bestUnp2() >= _minsc[1-rdi] &&
                                   _concordantPairs.size() > 0) return false;
                                else continue;
                            } else {
                                return false;
                            }
                        }
                    }
                } else {
                    assert(_paired);
                    assert_eq(rdi, 1);
                    bestScore = sink.bestUnp2();
                    if(bestScore >= _minsc[rdi]) {
                        // do not further extend this alignment
                        // unless it may be at least as good as the previous alignemnt
                        index_t maxmm = (-bestScore + sc.mmpMax - 1) / sc.mmpMax;
                        if(numSearched > maxmm + sink.bestSplicedUnp2() + 1) {
                            hit.done(true);
                            if(_paired) {
                                if(sink.bestUnp1() >= _minsc[1-rdi] &&
                                   _concordantPairs.size() > 0) return false;
                                else continue;
                            } else {
                                return false;
                            }
                        }
                    }
                }
                
                ReadBWTHit<index_t>& rchit = _hits[rdi][1-fwi];
                if(rchit.done() && bestScore < _minsc[rdi]) {
                    if(numSearched > rchit.numActualPartialSearch() + (anchorStop ? 1 : 0)) {
                        hit.done(true);
                        return false;
                    }
                }
            }

            // align this read beginning from previously stopped base
            // stops when it is uniquelly mapped with at least 28bp or
            // it may involve processed pseudogene
            partialSearch(
                          ebwtFw,
                          *_rds[rdi],
                          sc,
                          fw,
                          0,
                          mineFw,
                          mineRc,
                          hit,
                          rnd,
                          pseudogeneStop,
                          anchorStop);

            assert(hit.repOk());
            if(hit.done()) return true;
            // advance hit._cur by 1
            if(!pseudogeneStop) {
                if(hit._cur + 1 < hit._len) hit._cur++;
            }
            if(anchorStop) {
                hit.done(true);
                return true;
            }
            // hit.adjustOffset(_minK);
        }
        
        return false;
    }
    
    /**
     * Given partial alignments of a read, try to further extend
     * the alignment bidirectionally
     */
    virtual
    bool align(
               const Scoring&                   sc,
               const Ebwt<index_t>&             ebwtFw,
               const Ebwt<index_t>&             ebwtBw,
               const BitPairReference&          ref,
               SwAligner&                       swa,
               SpliceSiteDB&                    ssdb,
               index_t                          rdi,
               bool                             fw,
               WalkMetrics&                     wlm,
               PerReadMetrics&                  prm,
               SwMetrics&                       swm,
               HIMetrics&                       him,
               RandomSource&                    rnd,
               AlnSinkWrap<index_t>&            sink);
    
    /**
     * Given the alignment of its mate as an anchor,
     * align the read
     */
    virtual
    bool alignMate(
                   const Scoring&                   sc,
                   const Ebwt<index_t>&             ebwtFw,
                   const Ebwt<index_t>&             ebwtBw,
                   const BitPairReference&          ref,
                   SwAligner&                       swa,
                   SpliceSiteDB&                    ssdb,
                   index_t                          rdi,
                   bool                             fw,
                   WalkMetrics&                     wlm,
                   PerReadMetrics&                  prm,
                   SwMetrics&                       swm,
                   HIMetrics&                       him,
                   RandomSource&                    rnd,
                   AlnSinkWrap<index_t>&            sink,
                   index_t                          tidx,
                   index_t                          toff);
    
    /**
     * Given a partial alignment of a read, try to further extend
     * the alignment bidirectionally using a combination of
     * local search, extension, and global search
     */
    virtual
    void hybridSearch(
                      const Scoring&                     sc,
                      const Ebwt<index_t>&               ebwtFw,
                      const Ebwt<index_t>&               ebwtBw,
                      const BitPairReference&            ref,
                      SwAligner&                         swa,
                      SpliceSiteDB&                      ssdb,
                      index_t                            rdi,
                      bool                               fw,
                      WalkMetrics&                       wlm,
                      PerReadMetrics&                    prm,
                      SwMetrics&                         swm,
                      HIMetrics&                         him,
                      RandomSource&                      rnd,
                      AlnSinkWrap<index_t>&              sink)
    {}
    
    /**
     * Given a partial alignment of a read, try to further extend
     * the alignment bidirectionally using a combination of
     * local search, extension, and global search
     */
    virtual
    int64_t hybridSearch_recur(
                               const Scoring&                   sc,
                               const Ebwt<index_t>&             ebwtFw,
                               const Ebwt<index_t>&             ebwtBw,
                               const BitPairReference&          ref,
                               SwAligner&                       swa,
                               SpliceSiteDB&                    ssdb,
                               index_t                          rdi,
                               const GenomeHit<index_t>&        hit,
                               index_t                          hitoff,
                               index_t                          hitlen,
                               WalkMetrics&                     wlm,
                               PerReadMetrics&                  prm,
                               SwMetrics&                       swm,
                               HIMetrics&                       him,
                               RandomSource&                    rnd,
                               AlnSinkWrap<index_t>&            sink,
                               index_t                          dep = 0)
    { return numeric_limits<int64_t>::min(); }
    
    /**
     * Choose a candidate for alignment from a read or its reverse complement
     * (also from a mate or its reverse complement for pair)
     */
    bool pickNextReadToSearch(index_t& rdi, bool& fw) {
        rdi = 0; fw = true;
        bool picked = false;
        int64_t maxScore = std::numeric_limits<int64_t>::min();
        for(index_t rdi2 = 0; rdi2 < (_paired ? 2 : 1); rdi2++) {
            assert(_rds[rdi2] != NULL);
            for(index_t fwi = 0; fwi < 2; fwi++) {
                if     (fwi == 0 && _nofw[rdi2]) continue;
                else if(fwi == 1 && _norc[rdi2]) continue;

                if(_hits[rdi2][fwi].done()) continue;
                int64_t curScore = _hits[rdi2][fwi].searchScore(_minK);
                if(_hits[rdi2][fwi].cur() == 0) {
                    curScore = std::numeric_limits<int64_t>::max();
                }
                assert_gt(curScore, std::numeric_limits<int64_t>::min());
                if(curScore > maxScore) {
                    maxScore = curScore;
                    rdi = rdi2;
                    fw = (fwi == 0);
                    picked = true;
                }
            }
        }
        
        return picked;
    }

	/**
     * Align a part of a read without any edits
	 */
    size_t partialSearch(
                         const Ebwt<index_t>&    ebwt,    // BWT index
                         const Read&             read,    // read to align
                         const Scoring&          sc,      // scoring scheme
                         bool                    fw,      // don't align forward read
                         size_t                  mineMax, // don't care about edit bounds > this
                         size_t&                 mineFw,  // minimum # edits for forward read
                         size_t&                 mineRc,  // minimum # edits for revcomp read
                         ReadBWTHit<index_t>&    hit,     // holds all the seed hits (and exact hit)
                         RandomSource&           rnd,
                         bool&                   pseudogeneStop,  // stop if mapped to multiple locations due to processed pseudogenes
                         bool&                   anchorStop);
    
    /**
     * Global FM index search
	 */
	size_t globalEbwtSearch(
                            const Ebwt<index_t>& ebwt,  // BWT index
                            const Read&          read,  // read to align
                            const Scoring&       sc,    // scoring scheme
                            bool                 fw,
                            index_t              hitoff,
                            index_t&             hitlen,
                            index_t&             top,
                            index_t&             bot,
                            RandomSource&        rnd,
                            bool&                uniqueStop,
                            index_t              maxHitLen = (index_t)OFF_MASK);
    
    /**
     * Local FM index search
	 */
	size_t localEbwtSearch(
                           const LocalEbwt<local_index_t, index_t>*  ebwtFw,  // BWT index
                           const LocalEbwt<local_index_t, index_t>*  ebwtBw,  // BWT index
                           const Read&                      read,    // read to align
                           const Scoring&                   sc,      // scoring scheme
                           bool                             fw,
                           bool                             searchfw,
                           index_t                          rdoff,
                           index_t&                         hitlen,
                           local_index_t&                   top,
                           local_index_t&                   bot,
                           RandomSource&                    rnd,
                           bool&                            uniqueStop,
                           local_index_t                    minUniqueLen,
                           local_index_t                    maxHitLen = (local_index_t)OFF_MASK);
    
    /**
     * Local FM index search
	 */
	size_t localEbwtSearch_reverse(
                                   const LocalEbwt<local_index_t, index_t>*  ebwtFw,  // BWT index
                                   const LocalEbwt<local_index_t, index_t>*  ebwtBw,  // BWT index
                                   const Read&                      read,    // read to align
                                   const Scoring&                   sc,      // scoring scheme
                                   bool                             fw,
                                   bool                             searchfw,
                                   index_t                          rdoff,
                                   index_t&                         hitlen,
                                   local_index_t&                   top,
                                   local_index_t&                   bot,
                                   RandomSource&                    rnd,
                                   bool&                            uniqueStop,
                                   local_index_t                    minUniqueLen,
                                   local_index_t                    maxHitLen = (local_index_t)OFF_MASK);
    
    /**
     * Convert FM offsets to the corresponding genomic offset (chromosome id, offset)
     **/
    bool getGenomeCoords(
                         const Ebwt<index_t>&       ebwt,
                         const BitPairReference&    ref,
                         RandomSource&              rnd,
                         index_t                    top,
                         index_t                    bot,
                         bool                       fw,
                         index_t                    maxelt,
                         index_t                    rdoff,
                         index_t                    rdlen,
                         EList<Coord>&              coords,
                         WalkMetrics&               met,
                         PerReadMetrics&            prm,
                         HIMetrics&                 him,
                         bool                       rejectStraddle,
                         bool&                      straddled);
    
    /**
     * Convert FM offsets to the corresponding genomic offset (chromosome id, offset)
     **/
    bool getGenomeCoords_local(
                               const Ebwt<local_index_t>&   ebwt,
                               const BitPairReference&      ref,
                               RandomSource&                rnd,
                               local_index_t                top,
                               local_index_t                bot,
                               bool                         fw,
                               index_t                      rdoff,
                               index_t                      rdlen,
                               EList<Coord>&                coords,
                               WalkMetrics&                 met,
                               PerReadMetrics&              prm,
                               HIMetrics&                   him,
                               bool                         rejectStraddle,
                               bool&                        straddled);
    
    /**
     * Given a set of partial alignments for a read,
     * choose some that are longer and mapped to fewer places
     */
    index_t getAnchorHits(
                          const Ebwt<index_t>&              ebwt,
                          const BitPairReference&           ref,
                          RandomSource&                     rnd,
                          index_t                           rdi,
                          bool                              fw,
                          EList<GenomeHit<index_t> >&       genomeHits,
                          index_t                           maxGenomeHitSize,
                          SharedTempVars<index_t>&          sharedVars,
                          WalkMetrics&                      wlm,
                          PerReadMetrics&                   prm,
                          HIMetrics&                        him)
    {
        index_t fwi = (fw ? 0 : 1);
        assert_lt(rdi, 2);
        assert(_rds[rdi] != NULL);
        ReadBWTHit<index_t>& hit = _hits[rdi][fwi];
        assert(hit.done());
        index_t offsetSize = hit.offsetSize();
        assert_gt(offsetSize, 0);
        const index_t max_size = (hit._cur >= hit._len ? maxGenomeHitSize : 1);
        genomeHits.clear();
        for(size_t hi = 0; hi < offsetSize; hi++) {
            index_t hj = 0;
            for(; hj < offsetSize; hj++) {
                BWTHit<index_t>& partialHit_j = hit.getPartialHit(hj);
                if(partialHit_j.empty() ||
                   (partialHit_j._hit_type == CANDIDATE_HIT && partialHit_j.size() > max_size) ||
                   partialHit_j.hasGenomeCoords() ||
                   partialHit_j.len() <= _minK + 2) continue;
                else break;
            }
            if(hj >= offsetSize) break;
            for(index_t hk = hj + 1; hk < offsetSize; hk++) {
                BWTHit<index_t>& partialHit_j = hit.getPartialHit(hj);
                BWTHit<index_t>& partialHit_k = hit.getPartialHit(hk);
                if(partialHit_k.empty() ||
                   (partialHit_k._hit_type == CANDIDATE_HIT && partialHit_k.size() > max_size) ||
                   partialHit_k.hasGenomeCoords() ||
                   partialHit_k.len() <= _minK + 2) continue;
                
                if(partialHit_j._hit_type == partialHit_k._hit_type) {
                    if((partialHit_j.size() > partialHit_k.size()) ||
                       (partialHit_j.size() == partialHit_k.size() && partialHit_j.len() < partialHit_k.len())) {
                        hj = hk;
                    }
                } else {
                    if(partialHit_k._hit_type > partialHit_j._hit_type) {
                        hj = hk;
                    }
                }
            }
            BWTHit<index_t>& partialHit = hit.getPartialHit(hj);
            assert(!partialHit.hasGenomeCoords());
            bool straddled = false;
            getGenomeCoords(
                            ebwt,
                            ref,
                            rnd,
                            partialHit._top,
                            partialHit._bot,
                            fw,
                            partialHit._bot - partialHit._top,
                            hit._len - partialHit._bwoff - partialHit._len,
                            partialHit._len,
                            partialHit._coords,
                            wlm,
                            prm,
                            him,
                            false, // reject straddled
                            straddled);
            if(!partialHit.hasGenomeCoords()) continue;
            EList<Coord>& coords = partialHit._coords;
            assert_gt(coords.size(), 0);
            const index_t genomeHit_size = genomeHits.size();
            if(genomeHit_size + coords.size() > maxGenomeHitSize) {
                coords.shufflePortion(0, coords.size(), rnd);
            }
            for(index_t k = 0; k < coords.size(); k++) {
                const Coord& coord = coords[k];
                index_t len = partialHit._len;
                index_t rdoff = hit._len - partialHit._bwoff - len;
                bool overlapped = false;
                for(index_t l = 0; l < genomeHit_size; l++) {
                    GenomeHit<index_t>& genomeHit = genomeHits[l];
                    if(genomeHit.ref() != (index_t)coord.ref() || genomeHit.fw() != coord.fw()) continue;
                    assert_lt(genomeHit.rdoff(), hit._len);
                    assert_lt(rdoff, hit._len);
                    index_t hitoff = genomeHit.refoff() + hit._len - genomeHit.rdoff();
                    index_t hitoff2 = coord.off() + hit._len - rdoff;
                    if(abs((int64_t)hitoff - (int64_t)hitoff2) <= (int64_t)_maxIntronLen) {
                        overlapped = true;
                        genomeHit._hitcount++;
                        break;
                    }
                }
                if(!overlapped) {
                    genomeHits.expand();
                    genomeHits.back().init(
                                           coord.orient(),
                                           rdoff,
                                           straddled ? 1 : len,
                                           0, // trim5
                                           0, // trim3
                                           coord.ref(),
                                           coord.off(),
                                           _sharedVars);
                }
                if(partialHit._hit_type == CANDIDATE_HIT && genomeHits.size() >= maxGenomeHitSize) break;
            }
            if(partialHit._hit_type == CANDIDATE_HIT && genomeHits.size() >= maxGenomeHitSize) break;
        }
        return genomeHits.size();
    }
    
    bool pairReads(
                   const Scoring&          sc,
                   const Ebwt<index_t>&    ebwtFw,
                   const Ebwt<index_t>&    ebwtBw,
                   const BitPairReference& ref,
                   WalkMetrics&            wlm,
                   PerReadMetrics&         prm,
                   HIMetrics&              him,
                   RandomSource&           rnd,
                   AlnSinkWrap<index_t>&   sink);

    /**
     *
     **/
    bool reportHit(
                   const Scoring&                   sc,
                   const Ebwt<index_t>&             ebwt,
                   const BitPairReference&          ref,
                   const SpliceSiteDB&              ssdb,
                   AlnSinkWrap<index_t>&            sink,
                   index_t                          rdi,
                   const GenomeHit<index_t>&        hit,
                   const GenomeHit<index_t>*        ohit = NULL);
    
    /**
     * check this alignment is already examined
     **/
    bool redundant(
                   AlnSinkWrap<index_t>&    sink,
                   index_t                  rdi,
                   index_t                  tidx,
                   index_t                  toff);
    
    /**
     * check this alignment is already examined
     **/
    bool redundant(
                   AlnSinkWrap<index_t>&            sink,
                   index_t                          rdi,
                   const GenomeHit<index_t>&        hit);
    
    
    /**
     *
     **/
    bool isSearched(
                    const GenomeHit<index_t>&       hit,
                    index_t                         rdi);
    
    /**
     *
     **/
    void addSearched(const GenomeHit<index_t>&       hit,
                     index_t                         rdi);
    
    
protected:
  
    Read *   _rds[2];
    bool     _paired;
    bool     _rightendonly;
    bool     _nofw[2];
    bool     _norc[2];
    TAlScore _minsc[2];
    TAlScore _maxpen[2];
    
    size_t   _minIntronLen;
    size_t   _maxIntronLen;
    
    bool     _secondary;  // allow secondary alignments
    bool     _local;      // perform local alignments
    
    ReadBWTHit<index_t> _hits[2][2];
    
    EList<index_t, 16>                                 _offs;
    SARangeWithOffs<EListSlice<index_t, 16> >          _sas;
    GroupWalk2S<index_t, EListSlice<index_t, 16>, 16>  _gws;
    GroupWalkState<index_t>                            _gwstate;
    
    EList<local_index_t, 16>                                       _offs_local;
    SARangeWithOffs<EListSlice<local_index_t, 16> >                _sas_local;
    GroupWalk2S<local_index_t, EListSlice<local_index_t, 16>, 16>  _gws_local;
    GroupWalkState<local_index_t>                                  _gwstate_local;
            
    // temporary and shared variables used for GenomeHit
    // this should be defined before _genomeHits and _hits_searched
    SharedTempVars<index_t> _sharedVars;
    
    // temporary and shared variables for AlnRes
    LinkedEList<EList<Edit> > _rawEdits;
    
    // temporary
    EList<GenomeHit<index_t> >     _genomeHits;
    EList<bool>                    _genomeHits_done;
    ELList<Coord>                  _coords;
    ELList<SpliceSite>             _spliceSites;
    
    EList<pair<index_t, index_t> >  _concordantPairs;
    
    size_t _minK; // log4 of the size of a genome
    size_t _minK_local; // log4 of the size of a local index (8)

    ELList<GenomeHit<index_t> >     _local_genomeHits;
    EList<uint8_t>                  _anchors_added;
    uint64_t max_localindexatts;
    
	uint64_t bwops_;                    // Burrows-Wheeler operations
	uint64_t bwedits_;                  // Burrows-Wheeler edits
    
    //
    EList<GenomeHit<index_t> >     _hits_searched[2];
    
    uint64_t   _thread_rids_mindist;
    bool _no_spliced_alignment;

    // For AlnRes::matchesRef
	ASSERT_ONLY(EList<bool> raw_matches_);
	ASSERT_ONLY(BTDnaString tmp_rf_);
	ASSERT_ONLY(BTDnaString tmp_rdseq_);
	ASSERT_ONLY(BTString tmp_qseq_);
};

#define HIER_INIT_LOCS(top, bot, tloc, bloc, e) { \
	if(bot - top == 1) { \
		tloc.initFromRow(top, (e).eh(), (e).ebwt()); \
		bloc.invalidate(); \
	} else { \
		SideLocus<index_t>::initFromTopBot(top, bot, (e).eh(), (e).ebwt(), tloc, bloc); \
		assert(bloc.valid()); \
	} \
}

#define HIER_SANITY_CHECK_4TUP(t, b, tp, bp) { \
	ASSERT_ONLY(cur_index_t tot = (b[0]-t[0])+(b[1]-t[1])+(b[2]-t[2])+(b[3]-t[3])); \
	ASSERT_ONLY(cur_index_t totp = (bp[0]-tp[0])+(bp[1]-tp[1])+(bp[2]-tp[2])+(bp[3]-tp[3])); \
	assert_eq(tot, totp); \
}

#define LOCAL_INIT_LOCS(top, bot, tloc, bloc, e) { \
    if(bot - top == 1) { \
        tloc.initFromRow(top, (e).eh(), (e).ebwt()); \
        bloc.invalidate(); \
    } else { \
        SideLocus<local_index_t>::initFromTopBot(top, bot, (e).eh(), (e).ebwt(), tloc, bloc); \
        assert(bloc.valid()); \
    } \
}

/**
 * Given partial alignments of a read, try to further extend
 * the alignment bidirectionally
 */
template <typename index_t, typename local_index_t>
bool HI_Aligner<index_t, local_index_t>::align(
                                               const Scoring&                   sc,
                                               const Ebwt<index_t>&             ebwtFw,
                                               const Ebwt<index_t>&             ebwtBw,
                                               const BitPairReference&          ref,
                                               SwAligner&                       swa,
                                               SpliceSiteDB&                    ssdb,
                                               index_t                          rdi,
                                               bool                             fw,
                                               WalkMetrics&                     wlm,
                                               PerReadMetrics&                  prm,
                                               SwMetrics&                       swm,
                                               HIMetrics&                       him,
                                               RandomSource&                    rnd,
                                               AlnSinkWrap<index_t>&            sink)
{
    const ReportingParams& rp = sink.reportingParams();
    index_t fwi = (fw ? 0 : 1);
    assert_lt(rdi, 2);
    assert(_rds[rdi] != NULL);
    ReadBWTHit<index_t>& hit = _hits[rdi][fwi];
    assert(hit.done());
    index_t minOff = 0;
    if(hit.minWidth(minOff) > (index_t)(rp.khits * 2)) return false;
    
    // do not try to align if the potential alignment for this read might be
    // worse than the best alignment of its reverse complement
    int64_t bestScore = (rdi == 0 ? sink.bestUnp1() : sink.bestUnp2());
    index_t num_spliced = (rdi == 0 ? sink.bestSplicedUnp1() : sink.bestSplicedUnp2());
    if(bestScore < _minsc[rdi]) bestScore = _minsc[rdi];
    index_t maxmm = (-bestScore + sc.mmpMax - 1) / sc.mmpMax;
    index_t numActualPartialSearch = hit.numActualPartialSearch();
    if(!_secondary && numActualPartialSearch > maxmm + num_spliced + 1) return true;
    
    // choose candidate partial alignments for further alignment
    const index_t maxsize = rp.khits;
    index_t numHits = getAnchorHits(
                                    ebwtFw,
                                    ref,
                                    rnd,
                                    rdi,
                                    fw,
                                    _genomeHits,
                                    maxsize,
                                    _sharedVars,
                                    wlm,
                                    prm,
                                    him);
    if(numHits <= 0) return false;
   
    // limit the number of local index searches used for alignment of the read
    uint64_t add = 0;
    if(_secondary) add = (-_minsc[rdi] / sc.mmpMax) * numHits * 2;
    else           add = (-_minsc[rdi] / sc.mmpMax) * numHits;
    max_localindexatts = him.localindexatts + max<uint64_t>(10, add);
    // extend the partial alignments bidirectionally using
    // local search, extension, and (less often) global search
    hybridSearch(
                 sc,
                 ebwtFw,
                 ebwtBw,
                 ref,
                 swa,
                 ssdb,
                 rdi,
                 fw,
                 wlm,
                 prm,
                 swm,
                 him,
                 rnd,
                 sink);
    
    return true;
}


/**
 * Given the alignment of its mate as an anchor,
 * align the read
 */
template <typename index_t, typename local_index_t>
bool HI_Aligner<index_t, local_index_t>::alignMate(
                                                   const Scoring&                   sc,
                                                   const Ebwt<index_t>&             ebwtFw,
                                                   const Ebwt<index_t>&             ebwtBw,
                                                   const BitPairReference&          ref,
                                                   SwAligner&                       swa,
                                                   SpliceSiteDB&                    ssdb,
                                                   index_t                          rdi,
                                                   bool                             fw,
                                                   WalkMetrics&                     wlm,
                                                   PerReadMetrics&                  prm,
                                                   SwMetrics&                       swm,
                                                   HIMetrics&                       him,
                                                   RandomSource&                    rnd,
                                                   AlnSinkWrap<index_t>&            sink,
                                                   index_t                          tidx,
                                                   index_t                          toff)
{
    assert_lt(rdi, 2);
    index_t ordi = 1 - rdi;
    bool ofw = (fw == gMate2fw ? gMate1fw : gMate2fw);
    assert(_rds[ordi] != NULL);
    const Read& ord = *_rds[ordi];
    index_t rdlen = ord.length();
    assert_gt(rdlen, 0);
    
    _genomeHits.clear();
    if(_coords.size() == 0) {
        _coords.expand();
    }
    EList<Coord>& coords = _coords.front();
    
    // local search to find anchors
    const HierEbwt<index_t, local_index_t>* hierEbwt = (const HierEbwt<index_t, local_index_t>*)(&ebwtFw);
    const LocalEbwt<local_index_t, index_t>* localEbwt = hierEbwt->getLocalEbwt(tidx, toff);
    bool success = false, first = true;
    index_t count = 0;
    index_t max_hitlen = 0;
    while(!success && count++ < 2) {
        if(first) {
            first = false;
        } else {
            localEbwt = hierEbwt->prevLocalEbwt(localEbwt);
            if(localEbwt == NULL || localEbwt->empty()) break;
        }
        index_t hitoff = rdlen - 1;
        while(hitoff >= _minK_local - 1) {
            index_t hitlen = 0;
            local_index_t top = (local_index_t)OFF_MASK, bot = (local_index_t)OFF_MASK;
            bool uniqueStop = false;
            index_t nelt = localEbwtSearch(
                                           localEbwt,   // BWT index
                                           NULL,        // BWT index
                                           ord,          // read to align
                                           sc,          // scoring scheme
                                           ofw,
                                           false,       // searchfw,
                                           hitoff,
                                           hitlen,
                                           top,
                                           bot,
                                           rnd,
                                           uniqueStop,
                                           _minK_local);
            assert_leq(top, bot);
            assert_eq(nelt, (index_t)(bot - top));
            assert_leq(hitlen, hitoff + 1);
            if(nelt > 0 && nelt <= 5 && hitlen > max_hitlen) {
                coords.clear();
                bool straddled = false;
                getGenomeCoords_local(
                                      *localEbwt,
                                      ref,
                                      rnd,
                                      top,
                                      bot,
                                      ofw,
                                      hitoff - hitlen + 1,
                                      hitlen,
                                      coords,
                                      wlm,
                                      prm,
                                      him,
                                      true, // reject straddled?
                                      straddled);
                assert_leq(coords.size(), nelt);
                _genomeHits.clear();
                for(index_t ri = 0; ri < coords.size(); ri++) {
                    const Coord& coord = coords[ri];
                    _genomeHits.expand();
                    _genomeHits.back().init(
                                            coord.orient(),
                                            hitoff - hitlen + 1,
                                            hitlen,
                                            0, // trim5
                                            0, // trim3
                                            coord.ref(),
                                            coord.off(),
                                            _sharedVars);
                }
                max_hitlen = hitlen;
            }
            
            assert_leq(hitlen, hitoff + 1);
            hitoff -= (hitlen - 1);
            if(hitoff > 0) hitoff -= 1;
        } // while(hitoff >= _minK_local - 1)
    } // while(!success && count++ < 2)
    
    if(max_hitlen < _minK_local) return false;
    
    // randomly select
    const index_t maxsize = 5;
    if(_genomeHits.size() > maxsize) {
        _genomeHits.shufflePortion(0, _genomeHits.size(), rnd);
        _genomeHits.resize(maxsize);
    }
    
    // local search using the anchor
    for(index_t hi = 0; hi < _genomeHits.size(); hi++) {
        him.anchoratts++;
        GenomeHit<index_t>& genomeHit = _genomeHits[hi];
        index_t leftext = (index_t)OFF_MASK, rightext = (index_t)OFF_MASK;
        genomeHit.extend(
                         ord,
                         ref,
                         ssdb,
                         swa,
                         swm,
                         prm,
                         sc,
                         _minsc[ordi],
                         rnd,
                         _minK_local,
                         _minIntronLen,
                         _maxIntronLen,
                         leftext,
                         rightext);
        hybridSearch_recur(
                           sc,
                           ebwtFw,
                           ebwtBw,
                           ref,
                           swa,
                           ssdb,
                           ordi,
                           genomeHit,
                           genomeHit.rdoff(),
                           genomeHit.len(),
                           wlm,
                           prm,
                           swm,
                           him,
                           rnd,
                           sink);
    }
    
    return true;
}


/**
 * convert FM offsets to the corresponding genomic offset (chromosome id, offset)
 **/
template <typename index_t, typename local_index_t>
bool HI_Aligner<index_t, local_index_t>::getGenomeCoords(
                                                         const Ebwt<index_t>&       ebwt,
                                                         const BitPairReference&    ref,
                                                         RandomSource&              rnd,
                                                         index_t                    top,
                                                         index_t                    bot,
                                                         bool                       fw,
                                                         index_t                    maxelt,
                                                         index_t                    rdoff,
                                                         index_t                    rdlen,
                                                         EList<Coord>&              coords,
                                                         WalkMetrics&               met,
                                                         PerReadMetrics&            prm,
                                                         HIMetrics&                 him,
                                                         bool                       rejectStraddle,
                                                         bool&                      straddled)
{
    straddled = false;
    assert_gt(bot, top);
    index_t nelt = bot - top;
    nelt = min<index_t>(nelt, maxelt);
    coords.clear();
    him.globalgenomecoords += (bot - top);
    _offs.resize(nelt);
    _offs.fill(std::numeric_limits<index_t>::max());
    _sas.init(top, rdlen, EListSlice<index_t, 16>(_offs, 0, nelt));
    _gws.init(ebwt, ref, _sas, rnd, met);
    
    for(index_t off = 0; off < nelt; off++) {
        WalkResult<index_t> wr;
        index_t tidx = 0, toff = 0, tlen = 0;
        _gws.advanceElement(
                            off,
                            ebwt,         // forward Bowtie index for walking left
                            ref,          // bitpair-encoded reference
                            _sas,         // SA range with offsets
                            _gwstate,     // GroupWalk state; scratch space
                            wr,           // put the result here
                            met,          // metrics
                            prm);         // per-read metrics
        assert_neq(wr.toff, (index_t)OFF_MASK);
        bool straddled2 = false;
        ebwt.joinedToTextOff(
                             wr.elt.len,
                             wr.toff,
                             tidx,
                             toff,
                             tlen,
                             rejectStraddle,        // reject straddlers?
                             straddled2);  // straddled?
        
        straddled |= straddled2;
        
        if(tidx == (index_t)OFF_MASK) {
            // The seed hit straddled a reference boundary so the seed
            // hit isn't valid
            return false;
        }
        index_t global_toff = toff, global_tidx = tidx;
        if(global_toff < rdoff) continue;
        
        // Coordinate of the seed hit w/r/t the pasted reference string
        coords.expand();
        coords.back().init(global_tidx, (int64_t)global_toff, fw);
    }
    
    return true;
}

/**
 * convert FM offsets to the corresponding genomic offset (chromosome id, offset)
 **/
template <typename index_t, typename local_index_t>
bool HI_Aligner<index_t, local_index_t>::getGenomeCoords_local(
                                                               const Ebwt<local_index_t>&   ebwt,
                                                               const BitPairReference&      ref,
                                                               RandomSource&                rnd,
                                                               local_index_t                top,
                                                               local_index_t                bot,
                                                               bool                         fw,
                                                               index_t                      rdoff,
                                                               index_t                      rdlen,
                                                               EList<Coord>&                coords,
                                                               WalkMetrics&                 met,
                                                               PerReadMetrics&              prm,
                                                               HIMetrics&                   him,
                                                               bool                         rejectStraddle,
                                                               bool&                        straddled)
{
    straddled = false;
    assert_gt(bot, top);
    index_t nelt = bot - top;
    coords.clear();
    him.localgenomecoords += (bot - top);
    _offs_local.resize(nelt);
    _offs_local.fill(std::numeric_limits<local_index_t>::max());
    _sas_local.init(top, rdlen, EListSlice<local_index_t, 16>(_offs_local, 0, nelt));
    _gws_local.init(ebwt, ref, _sas_local, rnd, met);
    
    for(local_index_t off = 0; off < nelt; off++) {
        WalkResult<local_index_t> wr;
        local_index_t tidx = 0, toff = 0, tlen = 0;
        _gws_local.advanceElement(
                                  off,
                                  ebwt,         // forward Bowtie index for walking left
                                  ref,          // bitpair-encoded reference
                                  _sas_local,   // SA range with offsets
                                  _gwstate_local, // GroupWalk state; scratch space
                                  wr,           // put the result here
                                  met,          // metrics
                                  prm);         // per-read metrics
        assert_neq(wr.toff, (local_index_t)OFF_MASK);
        bool straddled2 = false;
        ebwt.joinedToTextOff(
                             wr.elt.len,
                             wr.toff,
                             tidx,
                             toff,
                             tlen,
                             rejectStraddle,        // reject straddlers?
                             straddled2);  // straddled?
        
        straddled |= straddled2;
        
        if(tidx == (local_index_t)OFF_MASK) {
            // The seed hit straddled a reference boundary so the seed
            // hit isn't valid
            return false;
        }
        index_t global_toff = toff, global_tidx = tidx;
        LocalEbwt<local_index_t, index_t>* localEbwt = (LocalEbwt<local_index_t, index_t>*)&ebwt;
        global_tidx = localEbwt->_tidx, global_toff = toff + localEbwt->_localOffset;
        if(global_toff < rdoff) continue;
        
        // Coordinate of the seed hit w/r/t the pasted reference string
        coords.expand();
        coords.back().init(global_tidx, (int64_t)global_toff, fw);
    }
    
    return true;
}


/**
 * examine alignments of left and right reads to produce concordant pair alignment
 **/
template <typename index_t, typename local_index_t>
bool HI_Aligner<index_t, local_index_t>::pairReads(
                                                   const Scoring&          sc,
                                                   const Ebwt<index_t>&    ebwtFw,
                                                   const Ebwt<index_t>&    ebwtBw,
                                                   const BitPairReference& ref,
                                                   WalkMetrics&            wlm,
                                                   PerReadMetrics&         prm,
                                                   HIMetrics&              him,
                                                   RandomSource&           rnd,
                                                   AlnSinkWrap<index_t>&   sink)
{
    assert(_paired);
    const EList<AlnRes> *rs1 = NULL, *rs2 = NULL;
    sink.getUnp1(rs1); assert(rs1 != NULL);
    sink.getUnp2(rs2); assert(rs2 != NULL);
    for(index_t i = 0; i < rs1->size(); i++) {
        for(index_t j = 0; j < rs2->size(); j++) {
            bool exists = false;
            for(index_t k = 0; k < _concordantPairs.size(); k++) {
                const pair<index_t, index_t>& p = _concordantPairs[k];
                if(i == p.first && j == p.second) {
                    exists = true;
                    break;
                }
            }
            if(exists) continue;
            if(sink.state().doneConcordant()) return true;
            const AlnRes& r1 = (*rs1)[i];
            Coord left = r1.refcoord(), right = r1.refcoord_right();
            assert_eq(left.ref(), right.ref());
            const AlnRes& r2 = (*rs2)[j];
            Coord left2 = r2.refcoord(), right2 = r2.refcoord_right();
            assert_eq(left2.ref(), right2.ref());
            if(left.ref() != left2.ref()) continue;
            assert_eq(left.orient(), right.orient());
            assert_eq(left2.orient(), right2.orient());
            if(left.orient() == gMate1fw) {
                if(left2.orient() != gMate2fw) continue;
            } else {
                if(left2.orient() == gMate2fw) continue;
                Coord temp = left; left = left2; left2 = temp;
                temp = right; right = right2; right2 = temp;
            }
            if(left.off() > left2.off()) continue;
            if(right.off() > right2.off()) continue;
            if(right.off() + (int)_maxIntronLen < left2.off()) continue;
            assert_geq(r1.score().score(), _minsc[0]);
            assert_geq(r2.score().score(), _minsc[1]);
            if(r1.score().score() + r2.score().score() >= sink.bestPair() || _secondary) {
                sink.report(0, &r1, &r2);
                _concordantPairs.expand();
                _concordantPairs.back().first = i;
                _concordantPairs.back().second = j;
            }
        }
    }
    return true;
}


/**
 * report read (or pair) alignment
 **/
template <typename index_t, typename local_index_t>
bool HI_Aligner<index_t, local_index_t>::reportHit(
                                                   const Scoring&                   sc,
                                                   const Ebwt<index_t>&             ebwt,
                                                   const BitPairReference&          ref,
                                                   const SpliceSiteDB&              ssdb,
                                                   AlnSinkWrap<index_t>&            sink,
                                                   index_t                          rdi,
                                                   const GenomeHit<index_t>&        hit,
                                                   const GenomeHit<index_t>*        ohit)
{
    assert_lt(rdi, 2);
    assert(_rds[rdi] != NULL);
    const Read& rd = *_rds[rdi];
    index_t rdlen = rd.length();
    if(hit.rdoff() - hit.trim5() > 0 || hit.len() + hit.trim5() + hit.trim3() < rdlen) return false;
    if(hit.score() < _minsc[rdi]) return false;
    
    // Edits are represented from 5' end of read to 3' end, not an alignment of read
    EList<Edit>& edits = const_cast<EList<Edit>&>(hit.edits());
    if(hit.trim5() > 0) {
        for(size_t i = 0; i < edits.size(); i++) {
            edits[i].pos += hit.trim5();
        }
    }
    if(!hit.fw()) {
        Edit::invertPoss(edits, rdlen, false);
    }
    // in case of multiple exonic alignments, choose the ones near (known) splice sites
    // this helps eliminate cases of reads being mapped to pseudogenes
    bool nearSpliceSites = hit.spliced();
    if(!this->_no_spliced_alignment) {
        if(!hit.spliced()) {
            const index_t max_exon_size = 2000;
            index_t left1 = 0, right1 = hit.refoff();
            if(right1 > max_exon_size) left1 = right1 - max_exon_size;
            index_t left2 = hit.refoff() + hit.len() - 1, right2 = left2 + max_exon_size;
            nearSpliceSites = ssdb.hasSpliceSites(
                                                  hit.ref(),
                                                  left1,
                                                  right1,
                                                  left2,
                                                  right2,
                                                  true); // include novel splice sites
        }
    }
    AlnScore asc(
                 hit.score(),  // numeric score
                 hit.ns(),     // # Ns
                 hit.ngaps(),  // # gaps
                 hit.splicescore(), // splice score
                 nearSpliceSites);
    bool softTrim = hit.trim5() > 0 || hit.trim3() > 0;
    AlnRes rs;
    rs.init(
            rdlen,                      // # chars after hard trimming
            asc,                        // alignment score
            &hit.edits(),               // nucleotide edits array
            0,                          // nucleotide edits first pos
            hit.edits().size(),         // nucleotide edits last pos
            NULL,                       // ambig base array
            0,                          // ambig base first pos
            0,                          // ambig base last pos
            hit.coord(),                // coord of leftmost aligned char in ref
            ebwt.plen()[hit.ref()],     // length of reference aligned to
            &_rawEdits,
            -1,                         // # seed mms allowed
            -1,                         // seed length
            -1,                         // seed interval
            0,                          // minimum score for valid alignment (daehwan)
            -1,                         // nuc5p (for colorspace)
            -1,                         // nuc3p (for colorspace)
            false,                      // soft pre-trimming?
            0,                          // 5p pre-trimming
            0,                          // 3p pre-trimming
            softTrim,                   // soft trimming?
            hit.fw() ? hit.trim5() : hit.trim3(),  // 5p trimming
            hit.fw() ? hit.trim3() : hit.trim5()); // 3p trimming
    if(!hit.fw()) {
        Edit::invertPoss(edits, rdlen, false);
    }
    if(hit.trim5() > 0) {
        for(size_t i = 0; i < edits.size(); i++) {
            edits[i].pos -= hit.trim5();
        }
    }
    //rs.setRefNs(nrefn);
    assert(rs.matchesRef(
                         rd,
                         ref,
                         tmp_rf_,
                         tmp_rdseq_,
                         tmp_qseq_,
                         _sharedVars.raw_refbuf,
                         _sharedVars.destU32,
                         raw_matches_,
                         _sharedVars.raw_refbuf2,
                         _sharedVars.reflens,
                         _sharedVars.refoffs));
    if(ohit == NULL) {
        bool done;
        if(rdi == 0 && !_rightendonly) {
            done = sink.report(0, &rs, NULL);
        } else {
            done = sink.report(0, NULL, &rs);
        }
        return done;
    }
    
    assert(ohit != NULL);
    const Read& ord = *_rds[1-rdi];
    index_t ordlen = ord.length();
    if(ohit->rdoff() - ohit->trim5() > 0 || ohit->len() + ohit->trim5() + ohit->trim3() < ordlen) return false;
    if(ohit->score() < _minsc[1-rdi]) return false;
    EList<Edit>& oedits = const_cast<EList<Edit>&>(ohit->edits());
    if(ohit->trim5() > 0) {
        for(size_t i = 0; i < oedits.size(); i++) {
            oedits[i].pos += ohit->trim5();
        }
    }
    if(!ohit->fw()) {
        Edit::invertPoss(oedits, ordlen, false);
    }
    AlnScore oasc(
                  ohit->score(),  // numeric score
                  ohit->ns(),     // # Ns
                  ohit->ngaps()); // # gaps
    bool osoftTrim = ohit->trim5() > 0 || ohit->trim3() > 0;
    AlnRes ors;
    ors.init(
             ordlen,                     // # chars after hard trimming
             oasc,                       // alignment score
             &ohit->edits(),             // nucleotide edits array
             0,                          // nucleotide edits first pos
             ohit->edits().size(),       // nucleotide edits last pos
             NULL,                       // ambig base array
             0,                          // ambig base first pos
             0,                          // ambig base last pos
             ohit->coord(),              // coord of leftmost aligned char in ref
             ebwt.plen()[ohit->ref()],   // length of reference aligned to
             &_rawEdits,
             -1,                         // # seed mms allowed
             -1,                         // seed length
             -1,                         // seed interval
             0,                          // minimum score for valid alignment (daehwan)
             -1,                         // nuc5p (for colorspace)
             -1,                         // nuc3p (for colorspace)
             false,                      // soft pre-trimming?
             0,                          // 5p pre-trimming
             0,                          // 3p pre-trimming
             osoftTrim,                  // soft trimming?
             ohit->fw() ? ohit->trim5() : ohit->trim3(),  // 5p trimming
             ohit->fw() ? ohit->trim3() : ohit->trim5()); // 3p trimming
    if(!ohit->fw()) {
        Edit::invertPoss(oedits, ordlen, false);
    }
    if(ohit->trim5() > 0) {
        for(size_t i = 0; i < oedits.size(); i++) {
            oedits[i].pos -= ohit->trim5();
        }
    }
    //rs.setRefNs(nrefn);
    assert(ors.matchesRef(
                          ord,
                          ref,
                          tmp_rf_,
                          tmp_rdseq_,
                          tmp_qseq_,
                          _sharedVars.raw_refbuf,
                          _sharedVars.destU32,
                          raw_matches_,
                          _sharedVars.raw_refbuf2,
                          _sharedVars.reflens,
                          _sharedVars.refoffs));
    
    bool done;
    if(rdi == 0) {
        done = sink.report(0, &rs, &ors);
    } else {
        done = sink.report(0, &ors, &rs);
    }
    return done;
}

/**
 * check this alignment is already examined
 **/
template <typename index_t, typename local_index_t>
bool HI_Aligner<index_t, local_index_t>::redundant(
                                                   AlnSinkWrap<index_t>&    sink,
                                                   index_t                  rdi,
                                                   index_t                  tidx,
                                                   index_t                  toff)
{
    assert_lt(rdi, 2);
    const EList<AlnRes>* rs = NULL;
    if(rdi == 0) sink.getUnp1(rs);
    else         sink.getUnp2(rs);
    assert(rs != NULL);
    for(index_t i = 0; i < rs->size(); i++) {
        Coord coord_left = (*rs)[i].refcoord(), coord_right = (*rs)[i].refcoord_right();
        assert_eq(coord_left.ref(), coord_right.ref());
        assert_lt(coord_left.off(), coord_right.off());
        assert_eq(coord_left.orient(), coord_right.orient());
        
        if(tidx != coord_left.ref()) continue;
        if(toff >= coord_left.off() && toff <= coord_right.off()) return true;
    }
    
    return false;
}


/**
 * check this alignment is already examined
 **/
template <typename index_t, typename local_index_t>
bool HI_Aligner<index_t, local_index_t>::redundant(
                                                   AlnSinkWrap<index_t>&            sink,
                                                   index_t                          rdi,
                                                   const GenomeHit<index_t>&        hit)
{
    assert_lt(rdi, 2);
    assert(_rds[rdi] != NULL);
    index_t rdlen = _rds[rdi]->length();
    const EList<AlnRes>* rs = NULL;
    if(rdi == 0) sink.getUnp1(rs);
    else         sink.getUnp2(rs);
    assert(rs != NULL);
    for(index_t i = 0; i < rs->size(); i++) {
        const AlnRes& rsi = (*rs)[i];
        if(rsi.refcoord() == hit.coord()) {
            const EList<Edit>& editsi = rsi.ned();
            const EList<Edit>& edits = hit.edits();
            if(editsi.size() == edits.size()) {
                size_t eidx = 0;
                if(!hit.fw()) {
                    Edit::invertPoss(const_cast<EList<Edit>&>(edits), rdlen, false);
                }
                for(; eidx < editsi.size(); eidx++) {
                    if(!(editsi[eidx] == edits[eidx])) {
                        break;
                    }
                }
                if(!hit.fw()) {
                    Edit::invertPoss(const_cast<EList<Edit>&>(edits), rdlen, false);
                }
                if(eidx >= editsi.size()) {
                    assert_eq(eidx, editsi.size());
                    return true;
                }
            }
        }
    }
    
    return false;
}


/**
 * Sweep right-to-left and left-to-right using exact matching.  Remember all
 * the SA ranges encountered along the way.  Report exact matches if there are
 * any.  Calculate a lower bound on the number of edits in an end-to-end
 * alignment.
 */
template <typename index_t, typename local_index_t>
size_t HI_Aligner<index_t, local_index_t>::partialSearch(
                                                         const Ebwt<index_t>&      ebwt,    // BWT index
                                                         const Read&               read,    // read to align
                                                         const Scoring&            sc,      // scoring scheme
                                                         bool                      fw,
                                                         size_t                    mineMax, // don't care about edit bounds > this
                                                         size_t&                   mineFw,  // minimum # edits for forward read
                                                         size_t&                   mineRc,  // minimum # edits for revcomp read
                                                         ReadBWTHit<index_t>&      hit,     // holds all the seed hits (and exact hit)
                                                         RandomSource&             rnd,     // pseudo-random source
                                                         bool&                     pseudogeneStop,
                                                         bool&                     anchorStop)
{
    bool pseudogeneStop_ = pseudogeneStop, anchorStop_ = anchorStop;
    pseudogeneStop = anchorStop = false;
	const index_t ftabLen = ebwt.eh().ftabChars();
	SideLocus<index_t> tloc, bloc;
	const index_t len = (index_t)read.length();
    const BTDnaString& seq = fw ? read.patFw : read.patRc;
    assert(!seq.empty());
    
    size_t nelt = 0;
    EList<BWTHit<index_t> >& partialHits = hit._partialHits;
    index_t& cur = hit._cur;
    assert_lt(cur, hit._len);
    
    hit._numPartialSearch++;
    
    index_t offset = cur;
    index_t dep = offset;
    index_t top = 0, bot = 0;
    index_t topTemp = 0, botTemp = 0;
    index_t left = len - dep;
    assert_gt(left, 0);
    if(left < ftabLen) {
        cur = hit._len;
        partialHits.expand();
        partialHits.back().init((index_t)OFF_MASK,
                                (index_t)OFF_MASK,
                                fw,
                                (index_t)offset,
                                (index_t)(cur - offset));
        hit.done(true);
		return 0;
    }
    // Does N interfere with use of Ftab?
    for(index_t i = 0; i < ftabLen; i++) {
        int c = seq[len-dep-1-i];
        if(c > 3) {
            cur += (i+1);
            partialHits.expand();
            partialHits.back().init((index_t)OFF_MASK,
                                    (index_t)OFF_MASK,
                                    fw,
                                    (index_t)offset,
                                    (index_t)(cur - offset));
            if(cur >= hit._len) {
                hit.done(true);
            }
			return 0;
        }
    }
    
    // Use ftab
    ebwt.ftabLoHi(seq, len - dep - ftabLen, false, top, bot);
    dep += ftabLen;
    if(bot <= top) {
        cur = dep;
        partialHits.expand();
        partialHits.back().init((index_t)OFF_MASK,
                                (index_t)OFF_MASK,
                                fw,
                                (index_t)offset,
                                (index_t)(cur - offset));
        if(cur >= hit._len) {
            hit.done(true);
        }
        return 0;
    }
    index_t same_range = 0, similar_range = 0;
    HIER_INIT_LOCS(top, bot, tloc, bloc, ebwt);
    // Keep going
    while(dep < len) {
        int c = seq[len-dep-1];
        if(c > 3) {
            topTemp = botTemp = 0;
        } else {
            if(bloc.valid()) {
                bwops_ += 2;
                topTemp = ebwt.mapLF(tloc, c);
                botTemp = ebwt.mapLF(bloc, c);
            } else {
                bwops_++;
                topTemp = ebwt.mapLF1(top, tloc, c);
                if(topTemp == (index_t)OFF_MASK) {
                    topTemp = botTemp = 0;
                } else {
                    botTemp = topTemp + 1;
                }
            }
        }
        if(botTemp <= topTemp) {
            break;
        }

        if(pseudogeneStop_) {
            if(botTemp - topTemp < bot - top && bot - top <= 5) {
                static const index_t minLenForPseudogene = _minK + 6;
                if(dep - offset >= minLenForPseudogene && similar_range >= 5) {
                    hit._numUniqueSearch++;
                    pseudogeneStop = true;
                    break;
                }
            }
            if(botTemp - topTemp != 1) {
                if(botTemp - topTemp + 2 >= bot - top) similar_range++;
                else if(botTemp - topTemp + 4 < bot - top) similar_range = 0;
            } else {
                pseudogeneStop_ = false;
            }
        }
        
        if(anchorStop_) {
            if(botTemp - topTemp != 1 && bot - top == botTemp - topTemp) {
                same_range++;
                if(same_range >= 5) {
                    anchorStop_ = false;
                }
            } else {
                same_range = 0;
            }
        
            if(dep - offset >= _minK + 8 && botTemp - topTemp >= 4) {
                anchorStop_ = false;
            }
        }
        
        top = topTemp;
        bot = botTemp;
        dep++;

        if(anchorStop_) {
            if(dep - offset >= _minK + 12 && bot - top == 1) {
                hit._numUniqueSearch++;
                anchorStop = true;
                break;
            }
        }
        
        HIER_INIT_LOCS(top, bot, tloc, bloc, ebwt);
    }
    
    // Done
    if(bot > top) {
        // This is an exact hit
        assert_gt(dep, offset);
        assert_leq(dep, len);
        partialHits.expand();
        index_t hit_type = CANDIDATE_HIT;
        if(anchorStop) hit_type = ANCHOR_HIT;
        else if(pseudogeneStop) hit_type = PSEUDOGENE_HIT;
        partialHits.back().init(top,
                                bot,
                                fw,
                                (index_t)offset,
                                (index_t)(dep - offset),
                                hit_type);
        
        nelt += (bot - top);
        cur = dep;
        if(cur >= hit._len) {
            if(hit_type == CANDIDATE_HIT) hit._numUniqueSearch++;
            hit.done(true);
        }
    }
    return nelt;
}


/**
 */
template <typename index_t, typename local_index_t>
size_t HI_Aligner<index_t, local_index_t>::globalEbwtSearch(
                                                            const Ebwt<index_t>& ebwt,  // BWT index
                                                            const Read&          read,  // read to align
                                                            const Scoring&       sc,    // scoring scheme
                                                            bool                 fw,
                                                            index_t              hitoff,
                                                            index_t&             hitlen,
                                                            index_t&             top,
                                                            index_t&             bot,
                                                            RandomSource&        rnd,
                                                            bool&                uniqueStop,
                                                            index_t              maxHitLen)
{
    bool uniqueStop_ = uniqueStop;
    uniqueStop = false;
    const index_t ftabLen = ebwt.eh().ftabChars();
	SideLocus<index_t> tloc, bloc;
	const index_t len = (index_t)read.length();
    
	size_t nelt = 0;
    const BTDnaString& seq = fw ? read.patFw : read.patRc;
    assert(!seq.empty());
    
    index_t offset = len - hitoff - 1;
    index_t dep = offset;
    top = 0, bot = 0;
    index_t topTemp = 0, botTemp = 0;
    index_t left = len - dep;
    assert_gt(left, 0);
    if(left < ftabLen) {
#if 1
        hitlen = left;
        return 0;
#else
        // Use fchr
        int c = seq[len-dep-1];
        if(c < 4) {
            top = ebwt.fchr()[c];
            bot = ebwt.fchr()[c+1];
        } else {
            hitlen = left;
            return 0;
        }
        dep++;
#endif
    } else {
        // Does N interfere with use of Ftab?
        for(index_t i = 0; i < ftabLen; i++) {
            int c = seq[len-dep-1-i];
            if(c > 3) {
                hitlen = (i+1);
                return 0;
            }
        }
        
        // Use ftab
        ebwt.ftabLoHi(seq, len - dep - ftabLen, false, top, bot);
        dep += ftabLen;
        if(bot <= top) {
            hitlen = ftabLen;
            return 0;
        }
    }
    
    HIER_INIT_LOCS(top, bot, tloc, bloc, ebwt);
    // Keep going
    while(dep < len) {
        int c = seq[len-dep-1];
        if(c > 3) {
            topTemp = botTemp = 0;
        } else {
            if(bloc.valid()) {
                bwops_ += 2;
                topTemp = ebwt.mapLF(tloc, c);
                botTemp = ebwt.mapLF(bloc, c);
            } else {
                bwops_++;
                topTemp = ebwt.mapLF1(top, tloc, c);
                if(topTemp == (index_t)OFF_MASK) {
                    topTemp = botTemp = 0;
                } else {
                    botTemp = topTemp + 1;
                }
            }
        }
        if(botTemp <= topTemp) {
            break;
        }
        
        top = topTemp;
        bot = botTemp;
        dep++;
        
        if(uniqueStop_) {
            if(bot - top == 1 && dep - offset >= _minK) {
                uniqueStop = true;
                break;
            }
        }
        
        HIER_INIT_LOCS(top, bot, tloc, bloc, ebwt);
    }
    
    // Done
    if(bot > top) {
        assert_gt(dep, offset);
        assert_leq(dep, len);
        nelt += (bot - top);
        hitlen = dep - offset;
    }
    return nelt;
}


/**
 *
 **/
template <typename index_t, typename local_index_t>
size_t HI_Aligner<index_t, local_index_t>::localEbwtSearch(
                                                           const LocalEbwt<local_index_t, index_t>*  ebwtFw,  // BWT index
                                                           const LocalEbwt<local_index_t, index_t>*  ebwtBw,  // BWT index
                                                           const Read&                      read,    // read to align
                                                           const Scoring&                   sc,      // scoring scheme
                                                           bool                             fw,
                                                           bool                             searchfw,
                                                           index_t                          rdoff,
                                                           index_t&                         hitlen,
                                                           local_index_t&                   top,
                                                           local_index_t&                   bot,
                                                           RandomSource&                    rnd,
                                                           bool&                         	uniqueStop,
                                                           local_index_t                    minUniqueLen,
                                                           local_index_t                    maxHitLen)
{
#ifndef NDEBUG
    if(searchfw) {
        assert(ebwtBw != NULL);
    } else {
        assert(ebwtFw != NULL);
    }
#endif
    bool uniqueStop_ = uniqueStop;
    uniqueStop = false;
    const LocalEbwt<local_index_t, index_t>& ebwt = *(searchfw ? ebwtBw : ebwtFw);
	const local_index_t ftabLen = (local_index_t)ebwt.eh().ftabChars();
	SideLocus<local_index_t> tloc, bloc;
	const local_index_t len = (local_index_t)read.length();
	size_t nelt = 0;
    
    const BTDnaString& seq = fw ? read.patFw : read.patRc;
    assert(!seq.empty());
    
    local_index_t offset = searchfw ? rdoff : len - rdoff - 1;
    local_index_t dep = offset;
    top = 0, bot = 0;
    local_index_t topTemp = 0, botTemp = 0;
    local_index_t left = len - dep;
    assert_gt(left, 0);
    if(left < ftabLen) {
        hitlen = left;
		return 0;
    }
    // Does N interfere with use of Ftab?
    for(local_index_t i = 0; i < ftabLen; i++) {
        int c = searchfw ? seq[dep+i] : seq[len-dep-1-i];
        if(c > 3) {
            hitlen = i + 1;
			return 0;
        }
    }
    
    // Use ftab
    if(searchfw) {
        ebwt.ftabLoHi(seq, dep, false, top, bot);
    } else {
        ebwt.ftabLoHi(seq, len - dep - ftabLen, false, top, bot);
    }
    dep += ftabLen;
    if(bot <= top) {
        hitlen = ftabLen;
        return 0;
    }
    LOCAL_INIT_LOCS(top, bot, tloc, bloc, ebwt);
    // Keep going
    while(dep < len) {
        int c = searchfw ? seq[dep] : seq[len-dep-1];
        if(c > 3) {
            topTemp = botTemp = 0;
        } else {
            if(bloc.valid()) {
                bwops_ += 2;
                topTemp = ebwt.mapLF(tloc, c);
                botTemp = ebwt.mapLF(bloc, c);
            } else {
                bwops_++;
                topTemp = ebwt.mapLF1(top, tloc, c);
                if(topTemp == (local_index_t)OFF_MASK) {
                    topTemp = botTemp = 0;
                } else {
                    botTemp = topTemp + 1;
                }
            }
        }
        if(botTemp <= topTemp) {
            break;
        }
        top = topTemp;
        bot = botTemp;
        LOCAL_INIT_LOCS(top, bot, tloc, bloc, ebwt);
        dep++;

        if(uniqueStop_) {
            if(bot - top == 1 && dep - offset >= minUniqueLen) {
                uniqueStop = true;
                break;
            }
        }
        
        if(dep - offset >= maxHitLen) break;
    }
    
    // Done
    if(bot > top) {
        assert_gt(dep, offset);
        assert_leq(dep, len);
        nelt += (bot - top);
        hitlen = dep - offset;
    }

    return nelt;
}

/**
 *
 **/
template <typename index_t, typename local_index_t>
size_t HI_Aligner<index_t, local_index_t>::localEbwtSearch_reverse(
                                                                   const LocalEbwt<local_index_t, index_t>*  ebwtFw,  // BWT index
                                                                   const LocalEbwt<local_index_t, index_t>*  ebwtBw,  // BWT index
                                                                   const Read&                      read,    // read to align
                                                                   const Scoring&                   sc,      // scoring scheme
                                                                   bool                             fw,
                                                                   bool                             searchfw,
                                                                   index_t                          rdoff,
                                                                   index_t&                         hitlen,
                                                                   local_index_t&                   top,
                                                                   local_index_t&                   bot,
                                                                   RandomSource&                    rnd,
                                                                   bool&                         	uniqueStop,
                                                                   local_index_t                    minUniqueLen,
                                                                   local_index_t                    maxHitLen)
{
#ifndef NDEBUG
    if(searchfw) {
        assert(ebwtBw != NULL);
    } else {
        assert(ebwtFw != NULL);
    }
#endif
    bool uniqueStop_ = uniqueStop;
    uniqueStop = false;
    const LocalEbwt<local_index_t, index_t>& ebwt = *(searchfw ? ebwtBw : ebwtFw);
	const local_index_t ftabLen = (local_index_t)ebwt.eh().ftabChars();
	SideLocus<local_index_t> tloc, bloc;
	const local_index_t len = (local_index_t)read.length();
	size_t nelt = 0;
    
    const BTDnaString& seq = fw ? read.patFw : read.patRc;
    assert(!seq.empty());
    
    local_index_t offset = searchfw ? len - rdoff - 1 : rdoff;
    local_index_t dep = offset;
    top = 0, bot = 0;
    local_index_t topTemp = 0, botTemp = 0;
    local_index_t left = len - dep;
    assert_gt(left, 0);
    if(left < ftabLen) {
        hitlen = left;
		return 0;
    }
    // Does N interfere with use of Ftab?
    for(local_index_t i = 0; i < ftabLen; i++) {
        int c = searchfw ? seq[len-dep-1-i] : seq[dep+i];
        if(c > 3) {
            hitlen = i + 1;
			return 0;
        }
    }
    
    // Use ftab
    if(searchfw) {
        ebwt.ftabLoHi(seq, len - dep - ftabLen, false, top, bot);
    } else {
        ebwt.ftabLoHi(seq, dep, false, top, bot);
    }
    dep += ftabLen;
    if(bot <= top) {
        hitlen = ftabLen;
        return 0;
    }
    LOCAL_INIT_LOCS(top, bot, tloc, bloc, ebwt);
    // Keep going
    while(dep < len) {
        int c = searchfw ? seq[len-dep-1] : seq[dep];
        if(c > 3) {
            topTemp = botTemp = 0;
        } else {
            if(bloc.valid()) {
                bwops_ += 2;
                topTemp = ebwt.mapLF(tloc, c);
                botTemp = ebwt.mapLF(bloc, c);
            } else {
                bwops_++;
                topTemp = ebwt.mapLF1(top, tloc, c);
                if(topTemp == (local_index_t)OFF_MASK) {
                    topTemp = botTemp = 0;
                } else {
                    botTemp = topTemp + 1;
                }
            }
        }
        if(botTemp <= topTemp) {
            break;
        }
        top = topTemp;
        bot = botTemp;
        LOCAL_INIT_LOCS(top, bot, tloc, bloc, ebwt);
        dep++;
        
        if(uniqueStop_) {
            if(bot - top == 1 && dep - offset >= minUniqueLen) {
                uniqueStop = true;
                break;
            }
        }
        
        if(dep - offset >= maxHitLen) break;
    }
    
    // Done
    if(bot > top) {
        assert_gt(dep, offset);
        assert_leq(dep, len);
        nelt += (bot - top);
        hitlen = dep - offset;
    }
    
    return nelt;
}

/**
 *
 **/
template <typename index_t, typename local_index_t>
bool HI_Aligner<index_t, local_index_t>::isSearched(
                                                    const GenomeHit<index_t>&   hit,
                                                    index_t                     rdi)
{
    assert_lt(rdi, 2);
    EList<GenomeHit<index_t> >& searchedHits = _hits_searched[rdi];
    for(index_t i = 0; i < searchedHits.size(); i++) {
        if(searchedHits[i].contains(hit)) return true;
    }
    return false;
}

/**
 *
 **/
template <typename index_t, typename local_index_t>
void HI_Aligner<index_t, local_index_t>::addSearched(
                                                     const GenomeHit<index_t>&   hit,
                                                     index_t                     rdi)
{
    assert_lt(rdi, 2);
    assert(!isSearched(hit, rdi));
    EList<GenomeHit<index_t> >& searchedHits = _hits_searched[rdi];
    searchedHits.push_back(hit);
}

#endif /*HI_ALIGNER_H_*/
