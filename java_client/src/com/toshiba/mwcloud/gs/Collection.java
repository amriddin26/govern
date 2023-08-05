/*
   Copyright (c) 2017 TOSHIBA Digital Solutions Corporation

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
package com.toshiba.mwcloud.gs;

import com.toshiba.mwcloud.gs.common.RowMapper;


/**
 * <div lang="ja">
 * ロウ集合を汎用的に管理するためのコンテナです。
 *
 * <p>ロウキーには次の型が使用できます。</p>
 * <ul>
 * <li>文字列型({@link String})</li>
 * <li>INTEGER型({@link Integer})</li>
 * <li>LONG型({@link Long})</li>
 * <li>TIMESTAMP型({@link java.util.Date})</li>
 * <li>上記の型のカラムを単一または複数持つ(複合)ロウキー({@link Row.Key})</li>
 * </ul>
 * <p>ロウキーの設定は必須ではありません。</p>
 *
 * <p>ロウ操作について、コンテナ固有の制限は設けられていません。</p>
 *
 * <p>{@link #query(String)}もしくは{@link GridStore#multiGet(java.util.Map)}
 * などより複数のロウの内容を一度に取得する場合、特に指定がなければ、
 * 返却されるロウの順序は不定となります。</p>
 *
 * <p>ロック粒度はロウ単位です。</p>
 * </div><div lang="en">
 * A general-purpose Container for managing a set of Rows.
 *
 * <p>The following types are available as a Row key.</p>
 * <ul>
 * <li>String type ({@link String})</li>
 * <li>INTEGER type ({@link Integer})</li>
 * <li>LONG type ({@link Long})</li>
 * <li>TIMESTAMP type ({@link java.util.Date})</li>
 * <li>Row key, or composite Row key ({@link Row.Key}), that has one or more
 * columns of the above type.</li>
 * </ul>
 * <p>It is not mandatory to set a Row key.</p>
 *
 * <p>There is no Container-specific constraint on Row operations. </p>
 *
 * <p>A set of Rows retuned by {@link #query(String)} or
 * {@link GridStore#multiGet(java.util.Map)} etc. in no particular order,
 * when order is not specified.</p>
 *
 * <p>The granularity of locking is a Row. </p>
 * </div>
 */
public interface Collection<K, R> extends Container<K, R> {

	/**
	 * <div lang="ja">
	 * 指定した空間範囲条件に合致するロウ集合を求めるための、クエリを作成します。
	 *
	 * <p>{@link Query#fetch(boolean)}を通じてロウ集合を求める際、
	 * 更新用ロックのオプションを有効にすることもできます。</p>
	 *
	 * <p>現バージョンでは、{@link GSException}や、{@code null}を指定できない
	 * 引数で{@code null}を指定したことによる{@link NullPointerException}は
	 * 送出されません。カラム名の誤りなどがあった場合、得られたクエリをフェッチする
	 * 際に例外が送出されます。</p>
	 *
	 * @param column 比較対象の空間型カラムの名前。{@code null}は指定できない
	 * @param geometry 比較対象として与える空間構造。{@code null}は指定できない
	 * @param geometryOp 比較方法。{@code null}は指定できない
	 *
	 * @throws GSException 現バージョンでは送出されない
	 * </div><div lang="en">
	 * Creates a query to obtain a set of Rows which are matched to specified
	 * geometry range conditions.
	 *
	 * <p>When obtaining a set of Rows using {@link Query#fetch(boolean)}, the
	 * option of locking for update can be enabled.</p>
	 *
	 * <p>{@link GSException} will not be thrown in the current version.
	 * If there is an error such as column names, exception will be thrown when
	 * fetching the obtained query.</p>
	 *
	 * <p>In the current version, {@link NullPointerException} will not be dispatched
	 * when {@link GSException} and {@code null} cannot be specified as {@code null}.
	 * If there is an error in the column name, etc., an exception is thrown when
	 * the obtained query is fetched. </p>
	 *
	 * @param column A name of the geometry type column to be compared. {@code null} cannot be specified
	 * @param geometry Geometry structure to be compared. {@code null} cannot be specified
	 * @param geometryOp Comparison method. {@code null} cannot be specified
	 *
	 * @throws GSException It will not be thrown in the current version.
	 * </div>
	 */
	public Query<R> query(
			String column, Geometry geometry, GeometryOperator geometryOp)
			throws GSException;

	/**
	 * <div lang="ja">
	 * 除外範囲付きの空間範囲条件に合致するロウ集合を求めるための、
	 * クエリを作成します。
	 *
	 * <p>{@code geometryIntersection}と交差し、かつ、{@code geometryDisjoint}と
	 * 交差しないカラム値を持つロウ集合を取得します。交差判定の条件は、
	 * {@link GeometryOperator#INTERSECT}と同一です。</p>
	 *
	 * <p>{@link Query#fetch(boolean)}を通じてロウ集合を求める際、
	 * 更新用ロックのオプションを有効にすることもできます。</p>
	 *
	 * <p>現バージョンでは、{@link GSException}や、{@code null}を指定できない
	 * 引数で{@code null}を指定したことによる{@link NullPointerException}は
	 * 送出されません。カラム名の誤りなどがあった場合、得られたクエリをフェッチする
	 * 際に例外が送出されます。</p>
	 *
	 * @param column 比較対象の空間型カラムの名前。{@code null}は指定できない
	 * @param geometryIntersection カラム上の値と交差する範囲を示す空間構造。
	 * {@code null}は指定できない
	 * @param geometryDisjoint カラム上の値と交差しない範囲を示す空間構造。
	 * {@code null}は指定できない
	 *
	 * @throws GSException 現バージョンでは送出されない
	 * </div><div lang="en">
	 * Creates a query to obtain a set of Rows which are matched to specified
	 * geometry range conditions with exclusion range.
	 *
	 * <p>Obtains a set of Rows which has the column values that intersect with
	 * {@code geometryIntersection} and do not intersect with
	 * {@code geometryDisjoint}. Conditions of the intersection determination is
	 * the same as the {@link GeometryOperator#INTERSECT}.
	 *
	 * <p>When obtaining a set of Rows using {@link Query#fetch(boolean)}, the
	 * option of locking for update can be enabled.</p>
	 *
	 * <p>In the current version, {@link NullPointerException} will not be dispatched
	 * when {@link GSException} and {@code null} cannot be specified as {@code null}.
	 * If there is an error in the column name, etc., an exception is thrown when
	 * the obtained query is fetched. </p>
	 *
	 * @param column A name of the geometry type column to be compared. {@code null} cannot be specified
	 * @param geometryIntersection Geometry structure indicating a range that
	 * intersects with the value on the column. {@code null} cannot be specified
	 * @param geometryDisjoint Geometry structure indicating a range that does
	 * not intersect with the values on the column. {@code null} cannot be specified
	 *
	 * @throws GSException It will not be thrown in the current version.
	 * </div>
	 */
	public Query<R> query(
			String column, Geometry geometryIntersection, Geometry geometryDisjoint)
			throws GSException;

	/**
	 * <div lang="ja">
	 * {@link Collection}ならびにその型パラメータと結びつく
	 * {@link Container.BindType}を構築するための、補助クラスです。
	 *
	 * @see Container.BindType
	 *
	 * @since 4.3
	 * </div><div lang="en">
	 * Auxiliary class for configuring {@link Container.BindType} which is associated
	 * with {@link Collection} and its type parameters.
	 *
	 * @see Container.BindType
	 *
	 * @since 4.3
	 * </div>
	 */
	public static class BindType {

		private BindType() {
		}

		/**
		 * <div lang="ja">
		 * 指定のロウオブジェクト型から得られるロウキーの型、指定の
		 * ロウオブジェクト型、ならびに、{@link Collection}と結びつく
		 * {@link Container.BindType}を取得します。
		 *
		 * @param <K> ロウキーの型
		 * @param <R> ロウオブジェクトの型
		 * @param rowClass ロウオブジェクトの型に対応するクラスオブジェクト
		 *
		 * @throws GSException 指定のロウオブジェクト型からロウキーの型が
		 * 得られなかった場合
		 *
		 * @since 4.3
		 * </div><div lang="en">
		 * Returns the Row key type obtained from the specified Row object type,
		 * the specified Row object type, and {@link Container.BindType} associated
		 * with the {@link Collection}.
		 *
		 * @param <K> Type of Row key
		 * @param <R> Type of Row object
		 * @param rowClass Class object corresponding to the type of the Row object
		 *
		 * @throws GSException if the Row key type cannot be obtained from the
		 * specified Row object type
		 *
		 * @since 4.3
		 * </div>
		 */
		public static <K, R extends Row.WithKey<K>> Container.BindType<
		K, R, ? extends Collection<K, R>> of(Class<R> rowClass)
				throws GSException {
			return of(
					RowMapper.BindingTool.resolveKeyClass(rowClass),
					rowClass);
		}

		/**
		 * <div lang="ja">
		 * 指定のロウキー型、指定のロウオブジェクト型、ならびに、
		 * {@link Collection}と結びつく{@link Container.BindType}を取得します。
		 *
		 * @param <K> ロウキーの型
		 * @param <R> ロウオブジェクトの型
		 * @param keyClass ロウキーの型に対応するクラスオブジェクト
		 * @param rowClass ロウオブジェクトの型に対応するクラスオブジェクト
		 *
		 * @throws GSException ロウキーの型と、ロウオブジェクトの型との間で
		 * 不整合を検出した場合
		 *
		 * @since 4.3
		 * </div><div lang="en">
		 * Returns the specified Row key type, the specified Row object type,
		 * and {@link Container.BindType} associated with the {@link Collection}.
		 *
		 * @param <K> Type of Row key
		 * @param <R> Type of Row object
		 * @param keyClass Class object corresponding to the type of Row key
		 * @param rowClass Class object corresponding to the type of the Row object
		 *
		 * @throws GSException if an inconsistency is detected between the type of
		 * Row key and the type of Row object
		 *
		 * @since 4.3
		 * </div>
		 */
		public static <K, R> Container.BindType<
		K, R, ? extends Collection<K, R>> of(
				Class<K> keyClass, Class<R> rowClass) throws GSException {
			return new Container.BindType<K, R, Collection<K, R>>(
					RowMapper.BindingTool.checkKeyClass(keyClass, rowClass),
					rowClass, Collection.class);
		}

		/**
		 * <div lang="ja">
		 * ロウキーを持たず、指定のロウオブジェクト型、ならびに、
		 * {@link Collection}と結びつく{@link Container.BindType}を取得します。
		 *
		 * @param <R> ロウオブジェクトの型
		 * @param rowClass ロウオブジェクトの型に対応するクラスオブジェクト
		 *
		 * @throws GSException ロウキーの型と、ロウオブジェクトの型との間で
		 * 不整合を検出した場合
		 *
		 * @since 4.3
		 * </div><div lang="en">
		 * Returns the {@link Container.BindType}, which has no Row key, associated with
		 * the specified Row object type and the {@link Collection}.
		 *
		 * @param <R> Type of Row object
		 * @param rowClass Class object corresponding to the type of the Row object
		 *
		 * @throws GSException if an inconsistency is detected between the type of
		 * Row key and the type of Row object
		 *
		 * @since 4.3
		 * </div>
		 */
		public static <R> Container.BindType<
		Void, R, ? extends Collection<Void, R>> noKeyOf(Class<R> rowClass)
				throws GSException {
			return of(Void.class, rowClass);
		}

	}

}
