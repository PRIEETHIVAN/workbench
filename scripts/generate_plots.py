#!/usr/bin/env python3
"""Create labeled SVG plots from solver-produced midpoint CSVs (stdlib only)."""
from csv import DictReader, writer
from pathlib import Path
import math
ROOT=Path(__file__).resolve().parents[1]; OUT=ROOT/'results'; MESHES=(10,20,40,80)
COLORS={10:'#e4572e',20:'#168aad',40:'#43aa8b',80:'#8e5ea2'}
def rows(n):
    with (OUT/f'fea_N{n}.csv').open(newline='',encoding='utf-8-sig') as f:return list(DictReader(f))
def start(title,w,h):return [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">','<rect width="100%" height="100%" fill="#fff"/>','<style>text{font-family:Arial,sans-serif;fill:#202733}.grid{stroke:#dce1e7}.axis{stroke:#39424e;stroke-width:2}.tick{font-size:12px}.legend{font-size:14px}.title{font-size:22px;font-weight:bold}.label{font-size:15px}</style>',f'<text class="title" x="{w/2}" y="32" text-anchor="middle">{title}</text>']
def line(pts,color,width=2,dash=None):
    d=f' stroke-dasharray="{dash}"' if dash else '';p=' '.join(f'{x:.2f},{y:.2f}' for x,y in pts);return f'<polyline fill="none" stroke="{color}" stroke-width="{width}"{d} points="{p}"/>'
# Temperature profile
W,H=960,620;L,R,T,B=90,760,70,500;X=lambda x:L+x*(R-L);Y=lambda y:B-y/200*(B-T);s=start('Steady 1D Rod Temperature: FE Midpoints vs Exact',W,H)
for t in range(0,201,20):
 y=Y(t);s += [f'<line class="grid" x1="{L}" y1="{y}" x2="{R}" y2="{y}"/>',f'<text class="tick" x="{L-12}" y="{y+4}" text-anchor="end">{t}</text>']
for x in [0,.2,.4,.6,.8,1]:
 p=X(x);s += [f'<line class="grid" x1="{p}" y1="{T}" x2="{p}" y2="{B}"/>',f'<text class="tick" x="{p}" y="{B+20}" text-anchor="middle">{x:.1f}</text>']
s += [f'<line class="axis" x1="{L}" y1="{B}" x2="{R}" y2="{B}"/>',f'<line class="axis" x1="{L}" y1="{T}" x2="{L}" y2="{B}"/>',f'<text class="label" x="{(L+R)/2}" y="{B+54}" text-anchor="middle">Position x (m)</text>',f'<text class="label" transform="translate(25 {(T+B)/2}) rotate(-90)" text-anchor="middle">Temperature T (°C)</text>']
s.append(line([(X(i/200),Y(100-80*i/200+500*(i/200)*(1-i/200))) for i in range(201)],'#111827',3,'8 5'))
for n in MESHES:
 ps=[(X(float(r['x_m'])),Y(float(r['T_FE_C']))) for r in rows(n)];s.append(line(ps,COLORS[n],2))
 for x,y in ps:s.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="2.2" fill="{COLORS[n]}"/>')
for i,(label,c,d) in enumerate([('Exact','#111827','8 5')]+[(f'N={n} FE',COLORS[n],None) for n in MESHES]):
 x,y=790,110+i*28;s += [line([(x,y),(x+25,y)],c,3,d),f'<text class="legend" x="{x+34}" y="{y+5}">{label}</text>']
s += ['<text class="tick" x="790" y="280">k=10 W/(m·K), q=10,000 W/m³</text>','<text class="tick" x="790" y="302">L=1 m, A=1e−4 m²</text>','<text class="tick" x="790" y="324">T(0)=100 °C, T(L)=20 °C</text>','</svg>'];(OUT/'fea_temperature_profiles.svg').write_text('\n'.join(s),encoding='utf-8')
# Convergence table and log-log plot
errors=[(n,max(abs(float(r['error_C'])) for r in rows(n))) for n in MESHES]
with (OUT/'fea_convergence.csv').open('w',newline='',encoding='utf-8') as f:w=writer(f);w.writerow(['n_elements','max_abs_midpoint_error_C']);w.writerows(errors)
W,H=820,560;L,R,T,B=95,750,70,445;X=lambda n:L+(math.log10(n)-math.log10(8))/(math.log10(100)-math.log10(8))*(R-L);Y=lambda e:B-(math.log10(e)-math.log10(.01))/(math.log10(2)-math.log10(.01))*(B-T);s=start('FE Mesh Convergence: Maximum Midpoint Error',W,H)
for e in [.01,.1,1]:
 y=Y(e);s += [f'<line class="grid" x1="{L}" y1="{y}" x2="{R}" y2="{y}"/>',f'<text class="tick" x="{L-10}" y="{y+4}" text-anchor="end">{e:g}</text>']
for n in MESHES:
 x=X(n);s += [f'<line class="grid" x1="{x}" y1="{T}" x2="{x}" y2="{B}"/>',f'<text class="tick" x="{x}" y="{B+20}" text-anchor="middle">{n}</text>']
s += [f'<line class="axis" x1="{L}" y1="{B}" x2="{R}" y2="{B}"/>',f'<line class="axis" x1="{L}" y1="{T}" x2="{L}" y2="{B}"/>',f'<text class="label" x="{(L+R)/2}" y="{B+55}" text-anchor="middle">Uniform elements N (log scale)</text>',f'<text class="label" transform="translate(25 {(T+B)/2}) rotate(-90)" text-anchor="middle">Maximum midpoint |T_FE−T_exact| (°C, log scale)</text>',line([(X(n),Y(e)) for n,e in errors],'#168aad',3)]
for n,e in errors:
 x,y=X(n),Y(e);s += [f'<circle cx="{x}" cy="{y}" r="5" fill="#e4572e"/>',f'<text class="tick" x="{x+8}" y="{y-8}">{e:.6g} °C</text>']
s.append('</svg>');(OUT/'fea_convergence.svg').write_text('\n'.join(s),encoding='utf-8');print('Wrote two SVG plots and convergence CSV.')
