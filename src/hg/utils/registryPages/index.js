(function(){
  var D  = JSON.parse(document.getElementById('tipdata').textContent);
  var AZ = JSON.parse(document.getElementById('azdata').textContent);
  var REGNAME = {track:'track cart variables', url:'URL parameters',
                 file:'file cart variables', conf:'hg.conf settings'};
  var tip = document.getElementById('tip');
  var cur = null;
  function esc(s){ var d=document.createElement('div'); d.textContent=s; return d.innerHTML; }

  function html_for(el){
    var j = el.getAttribute('data-j');
    if (j !== null){
      var e = AZ[+j]; if (!e) return '';
      var h = '<div class="tn">' + esc(e[0]) + '</div>';
      e[1].forEach(function(b){
        var k = b[0], rows = b[1];
        h += '<div class="trg"><i class="sh ' + k + '"></i>' + REGNAME[k] +
             (rows.length > 1 ? ' &middot; ' + rows.length + ' rows' : '') + '</div>';
        var ds = [], ms = [];
        rows.forEach(function(r){
          if (r[0] && ds.indexOf(r[0]) === -1) ds.push(r[0]);
          if (r[1] && ms.indexOf(r[1]) === -1) ms.push(r[1]);
        });
        ds.forEach(function(d){ h += '<p class="td">' + esc(d) + '</p>'; });
        if (ms.length){
          var shown = ms.slice(0, 6).map(esc);
          if (ms.length > 6) shown.push('+' + (ms.length - 6) + ' more');
          h += '<p class="tm">' + shown.join('<br>') + '</p>';
        }
      });
      return h;
    }
    var r = D[+el.getAttribute('data-i')]; if (!r) return '';
    var h2 = '<div class="tn">' + esc(r[0]) + '</div>';
    if (r[1]) h2 += '<p class="td">' + esc(r[1]) + '</p>';
    if (r[2]) h2 += '<p class="tm">' + esc(r[2]) + '</p>';
    return h2;
  }
  function show(el){
    var h = html_for(el); if (!h) return;
    tip.innerHTML = h;
    tip.setAttribute('data-on','1');
    tip.setAttribute('aria-hidden','false');
    cur = el;
    place(el);
  }
  function place(el){
    var r = el.getBoundingClientRect();
    var t = tip.getBoundingClientRect();
    var x = r.left;
    if (x + t.width > window.innerWidth - 12) x = window.innerWidth - 12 - t.width;
    if (x < 12) x = 12;
    var y = r.bottom + 8;
    if (y + t.height > window.innerHeight - 12) y = r.top - t.height - 8;
    if (y < 12) y = 12;
    tip.style.left = Math.round(x) + 'px';
    tip.style.top  = Math.round(y) + 'px';
  }
  function hide(){
    tip.removeAttribute('data-on');
    tip.setAttribute('aria-hidden','true');
    cur = null;
  }
  document.addEventListener('mouseover', function(e){
    var el = e.target.closest ? e.target.closest('.nm') : null;
    if (el) { if (el !== cur) show(el); }
    else if (cur && !cur.matches(':focus')) hide();
  });
  document.addEventListener('focusin', function(e){
    var el = e.target.closest ? e.target.closest('.nm') : null;
    if (el) show(el); else if (cur) hide();
  });
  document.addEventListener('keydown', function(e){ if (e.key === 'Escape') hide(); });
  window.addEventListener('scroll', function(){ if (cur) place(cur); }, {passive:true});
  window.addEventListener('resize', hide);

  /* view toggle */
  var vGroup = document.getElementById('vGroup'), vAz = document.getElementById('vAz');
  var regnav = document.getElementById('regnav');
  function setView(v, scroll){
    document.body.setAttribute('data-view', v);
    vGroup.setAttribute('aria-pressed', v === 'group' ? 'true' : 'false');
    vAz.setAttribute('aria-pressed', v === 'az' ? 'true' : 'false');
    regnav.style.display = v === 'group' ? '' : 'none';
    hide();
    if (scroll) window.scrollTo(0, 0);
    run();
  }
  vGroup.addEventListener('click', function(){ setView('group', true); });
  vAz.addEventListener('click', function(){ setView('az', true); });

  /* filter, scoped to the active view */
  var q = document.getElementById('q');
  var cnt = document.getElementById('cnt');
  var views = {group: document.getElementById('grouped'), az: document.getElementById('az')};
  var chips = {}, grps = {}, regs = {};
  Object.keys(views).forEach(function(k){
    chips[k] = Array.prototype.slice.call(views[k].querySelectorAll('.nm'));
    grps[k]  = Array.prototype.slice.call(views[k].querySelectorAll('.grp'));
    regs[k]  = Array.prototype.slice.call(views[k].querySelectorAll('.reg'));
  });
  function fmt(n){ return n.toLocaleString('en-US'); }
  function run(){
    var v = q.value.trim().toLowerCase();
    var view = document.body.getAttribute('data-view');
    var noun = view === 'az' ? ' names' : ' rows';
    var total = chips[view].length;
    hide();
    if (!v){
      document.body.classList.remove('filtering','noresult');
      cnt.textContent = fmt(total) + noun;
      return;
    }
    document.body.classList.add('filtering');
    var shown = 0;
    Object.keys(views).forEach(function(k){
      var n = 0;
      for (var i=0;i<chips[k].length;i++){
        var hit = chips[k][i].getAttribute('data-n').indexOf(v) !== -1;
        chips[k][i].classList.toggle('on', hit);
        if (hit) n++;
      }
      for (var g=0;g<grps[k].length;g++) grps[k][g].classList.toggle('on', !!grps[k][g].querySelector('.nm.on'));
      for (var r=0;r<regs[k].length;r++) regs[k][r].classList.toggle('on', !!regs[k][r].querySelector('.grp.on'));
      if (k === view) shown = n;
    });
    document.body.classList.toggle('noresult', shown === 0);
    cnt.textContent = fmt(shown) + ' of ' + fmt(total) + noun;
    if (window.scrollY > 0) window.scrollTo(0, 0);
  }
  q.addEventListener('input', run);
  setView('group', false);
})();
